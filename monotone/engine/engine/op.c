
//
// monotone
//
// time-series storage
//

#include <monotone_runtime.h>
#include <monotone_lib.h>
#include <monotone_config.h>
#include <monotone_io.h>
#include <monotone_storage.h>
#include <monotone_engine.h>
#include <malloc.h>

void
engine_refresh(Engine* self, Refresh* refresh, uint64_t min,
               bool if_exists)
{
	unused(self);
	refresh_reset(refresh);
	refresh_run(refresh, min, NULL, if_exists);
}

void
engine_refresh_range(Engine* self, Refresh* refresh, uint64_t min, uint64_t max)
{
	for (;;)
	{
		// get next ref >= min
		auto ref = engine_lock(self, min, LOCK_ACCESS, true, false);
		if (ref == NULL)
			return;
		uint64_t ref_min = ref->slice.min;
		uint64_t ref_max = ref->slice.max;
		engine_unlock(self, ref, LOCK_ACCESS);

		if (ref_min >= max)
			return;

		engine_refresh(self, refresh, ref_min, true);
		min = ref_max;
	}
}

void
engine_move(Engine* self, Refresh* refresh, uint64_t min, Str* storage,
            bool if_exists)
{
	unused(self);
	refresh_reset(refresh);
	refresh_run(refresh, min, storage, if_exists);
}

void
engine_move_range(Engine* self, Refresh* refresh, uint64_t min, uint64_t max,
                  Str* storage)
{
	for (;;)
	{
		// get next ref >= min
		auto ref = engine_lock(self, min, LOCK_ACCESS, true, false);
		if (ref == NULL)
			return;
		uint64_t ref_min = ref->slice.min;
		uint64_t ref_max = ref->slice.max;
		engine_unlock(self, ref, LOCK_ACCESS);

		if (ref_min >= max)
			return;

		engine_move(self, refresh, ref_min, storage, true);
		min = ref_max;
	}
}

static bool
engine_drop_file(Engine* self, uint64_t min, bool if_exists, int mask)
{
	// find the original partition
	auto ref = engine_lock(self, min, LOCK_SERVICE, false, false);
	if (unlikely(! ref))
	{
		if (! if_exists)
			error("drop: partition <%" PRIu64 "> not found", min);
		return false;
	}
	auto part = ref->part;

	// get access lock
	mutex_lock(&self->lock);
	ref_lock(ref, LOCK_ACCESS);

	// delete partition file on storage or cloud
	Exception e;
	if (try(&e))
	{
		// unless full drop, ensure that at least one copy remains
		bool full_drop = (mask == (PART_FILE|PART_FILE_CLOUD));

		if (mask & PART_FILE_CLOUD)
		{
			if (! full_drop)
			{
				// partition not in the storage
				if (! part_has(part, PART_FILE))
					error("drop: partition <%" PRIu64 "> must downloaded first", min);
			}

			if (part_has(part, PART_FILE_CLOUD))
			{
				part_offload(part, false);
				part_unset(part, PART_FILE_CLOUD);
			}
		}

		if (mask & PART_FILE)
		{
			if (! full_drop)
			{
				// partition not in the cloud
				if (! part_has(part, PART_FILE_CLOUD))
					error("drop: partition <%" PRIu64 "> must be uploaded first", min);
			}

			if (part_has(part, PART_FILE))
			{
				part_offload(part, true);
				part_unset(part, PART_FILE);
			}
		}
	}

	ref_unlock(ref, LOCK_ACCESS);
	mutex_unlock(&self->lock);

	engine_unlock(self, ref, LOCK_SERVICE);

	if (catch(&e))
		rethrow();

	return true;
}

static bool
engine_drop_reference(Engine* self, uint64_t min)
{
	// take exclusive control lock
	control_lock_exclusive();

	// find partition = min
	auto slice = mapping_match(&self->mapping, min);
	if (! slice)
	{
		control_unlock();
		return true;
	}
	auto ref = ref_of(slice);
	auto part = ref->part;

	// remove only empty partitions
	if (part->state != PART_FILE_NONE)
	{
		// retry truncate
		control_unlock();
		return false;
	}

	// remove partition from mapping
	mapping_remove(&self->mapping, slice);

	// remove partition from storage
	auto storage = storage_mgr_find(&self->storage_mgr, &part->source->name);
	storage_remove(storage, part);

	control_unlock();

	// free
	ref_free(ref);
	return true;
}

void
engine_drop(Engine* self, uint64_t min, bool if_exists, int mask)
{
	// partition drop
	if (mask == (PART_FILE|PART_FILE_CLOUD))
	{
		for (;;)
		{
			// in order to avoid heavy exclusive lock during drop,
			// separate file drop first
			auto exists = engine_drop_file(self, min, if_exists, mask);
			if (! exists)
				break;

			// try to drop partition mapping, or retry drop if
			// partition still has files
			if (engine_drop_reference(self, min))
				break;
		}

		return;
	}

	// drop partition file on storage or cloud only
	engine_drop_file(self, min, if_exists, mask);
}

void
engine_drop_range(Engine* self, uint64_t min, uint64_t max, int mask)
{
	for (;;)
	{
		// get next ref >= min
		auto ref = engine_lock(self, min, LOCK_ACCESS, true, false);
		if (ref == NULL)
			return;
		uint64_t ref_min = ref->slice.min;
		uint64_t ref_max = ref->slice.max;
		engine_unlock(self, ref, LOCK_ACCESS);

		if (ref_min >= max)
			return;

		engine_drop(self, ref_min, true, mask);
		min = ref_max;
	}
}

static inline Part*
engine_rebalance_tier(Engine* self, Tier* tier, Str* storage)
{
	if (tier->storage->list_count <= tier->config->capacity)
		return NULL;

	// get oldest partition (by psn)
	auto oldest = storage_oldest(tier->storage);

	// schedule partition drop
	if (list_is_last(&self->conveyor.list, &tier->link))
		return oldest;

	// schedule partition move to the next tier storage
	auto next = container_of(tier->link.next, Tier, link);
	str_copy(storage, &next->storage->source->name);
	return oldest;
}

static bool
engine_rebalance_next(Engine* self, uint64_t* min, Str* storage)
{
	// take control shared lock
	control_lock_shared();
	guard(lock_guard, control_unlock_guard, NULL);

	if (conveyor_empty(&self->conveyor))
		return false;

	mutex_lock(&self->lock);

	list_foreach(&self->conveyor.list)
	{
		auto tier = list_at(Tier, link);
		auto part = engine_rebalance_tier(self, tier, storage);
		if (part)
		{
			// mark partition for refresh to avoid concurrent rebalance
			// calls for the same partition
			if (part->refresh)
				continue;
			part->refresh = true;

			*min = part->id.min;
			mutex_unlock(&self->lock);
			return true;
		}
	}

	mutex_unlock(&self->lock);
	return false;
}

void
engine_rebalance(Engine* self, Refresh* refresh)
{
	for (;;)
	{
		Str storage;
		str_init(&storage);
		guard(guard, str_free, &storage);
		uint64_t min;
		if (! engine_rebalance_next(self, &min, &storage))
			break;
		if (str_empty(&storage))
			engine_drop(self, min, true, PART_FILE|PART_FILE_CLOUD);
		else
			engine_move(self, refresh, min, &storage, true);
	}
}

void
engine_checkpoint(Engine* self)
{
	// take exclusive control lock
	control_lock_exclusive();
	guard(guard, control_unlock_guard, NULL);

	// schedule refresh
	auto slice = mapping_min(&self->mapping);
	while (slice)
	{
		auto ref = ref_of(slice);
		if (!ref->part->refresh && ref->part->memtable->size > 0)
		{
			service_refresh(self->service, slice->min);
			ref->part->refresh = true;
		}
		slice = mapping_next(&self->mapping, slice);
	}

	malloc_trim(0);
}

void
engine_download(Engine* self, uint64_t min,
                bool    if_exists,
                bool    if_cloud)
{
	// find the original partition
	auto ref = engine_lock(self, min, LOCK_SERVICE, false, false);
	if (unlikely(! ref))
	{
		if (! if_exists)
			error("download: partition <%" PRIu64 "> not found", min);
		return;
	}
	auto part = ref->part;

	if (! part->cloud)
	{
		engine_unlock(self, ref, LOCK_SERVICE);
		if (! if_cloud)
			error("download: partition <%" PRIu64 "> has no associated cloud", min);
		return;
	}

	// partition file exists locally
	if (part_has(part, PART_FILE))
	{
		engine_unlock(self, ref, LOCK_SERVICE);
		return;
	}

	// download and open partition file
	Exception e;
	if (try(&e)) {
		part_download(part);
	}
	if (catch(&e))
	{
		engine_unlock(self, ref, LOCK_SERVICE);
		rethrow();
	}

	// update partition state
	mutex_lock(&self->lock);
	ref_lock(ref, LOCK_ACCESS);

	part_set(part, PART_FILE);

	ref_unlock(ref, LOCK_ACCESS);
	mutex_unlock(&self->lock);

	// complete
	engine_unlock(self, ref, LOCK_SERVICE);
}

void
engine_download_range(Engine* self, uint64_t min, uint64_t max, bool if_cloud)
{
	for (;;)
	{
		// get next ref >= min
		auto ref = engine_lock(self, min, LOCK_ACCESS, true, false);
		if (ref == NULL)
			return;
		uint64_t ref_min = ref->slice.min;
		uint64_t ref_max = ref->slice.max;
		engine_unlock(self, ref, LOCK_ACCESS);

		if (ref_min >= max)
			return;

		engine_download(self, ref_min, true, if_cloud);
		min = ref_max;
	}
}

void
engine_upload(Engine* self, uint64_t min,
              bool    if_exists,
              bool    if_cloud)
{
	// find the original partition
	auto ref = engine_lock(self, min, LOCK_SERVICE, false, false);
	if (unlikely(! ref))
	{
		if (! if_exists)
			error("upload: partition <%" PRIu64 "> not found", min);
		return;
	}
	auto part = ref->part;

	if (! part->cloud)
	{
		engine_unlock(self, ref, LOCK_SERVICE);
		if (! if_cloud)
			error("upload: partition <%" PRIu64 "> storage has no associated cloud", min);
		return;
	}

	// partition already uploaded
	if (part_has(part, PART_FILE_CLOUD))
	{
		engine_unlock(self, ref, LOCK_SERVICE);
		return;
	}

	// ensure partition file exists locally
	if (! part_has(part, PART_FILE))
	{
		engine_unlock(self, ref, LOCK_SERVICE);
		error("upload: partition <%" PRIu64 "> file not yet exists", min);
		return;
	}

	// create cloud file and upload data file
	Exception e;
	if (try(&e)) {
		part_upload(part);
	}
	if (catch(&e))
	{
		engine_unlock(self, ref, LOCK_SERVICE);
		rethrow();
	}

	// update partition state
	mutex_lock(&self->lock);
	ref_lock(ref, LOCK_ACCESS);

	part_set(part, PART_FILE_CLOUD);

	ref_unlock(ref, LOCK_ACCESS);
	mutex_unlock(&self->lock);

	// complete
	engine_unlock(self, ref, LOCK_SERVICE);
}

void
engine_upload_range(Engine* self, uint64_t min, uint64_t max, bool if_cloud)
{
	for (;;)
	{
		// get next ref >= min
		auto ref = engine_lock(self, min, LOCK_ACCESS, true, false);
		if (ref == NULL)
			return;
		uint64_t ref_min = ref->slice.min;
		uint64_t ref_max = ref->slice.max;
		engine_unlock(self, ref, LOCK_ACCESS);

		if (ref_min >= max)
			return;

		engine_upload(self, ref_min, true, if_cloud);
		min = ref_max;
	}
}
