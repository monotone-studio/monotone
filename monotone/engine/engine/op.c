
//
// monotone
//
// time-series storage
//

#include <monotone_runtime.h>
#include <monotone_lib.h>
#include <monotone_config.h>
#include <monotone_cloud.h>
#include <monotone_io.h>
#include <monotone_storage.h>
#include <monotone_wal.h>
#include <monotone_engine.h>
#include <malloc.h>

static inline bool
engine_foreach(Engine* self, uint64_t* min, uint64_t* min_next, uint64_t max)
{
	// get next reference >= min and < max
	auto ref = engine_lock(self, *min, LOCK_ACCESS, true, false);
	if (ref == NULL)
		return false;
	uint64_t ref_min = ref->slice.min;
	uint64_t ref_max = ref->slice.max;
	engine_unlock(self, ref, LOCK_ACCESS);
	if (ref_min >= max)
		return false;
	*min = ref_min;
	*min_next = ref_max;
	return true;
}

static bool
engine_drop_file(Engine* self, uint64_t min, bool if_exists, bool if_cloud, int mask)
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

	// execute drop only if file is on cloud
	if (if_cloud && !part_has(part, PART_CLOUD))
	{
		engine_unlock(self, ref, LOCK_SERVICE);
		return false;
	}

	// get access lock
	mutex_lock(&self->lock);
	ref_lock(ref, LOCK_ACCESS);
	mutex_unlock(&self->lock);

	// delete partition file on storage or cloud
	Exception e;
	if (try(&e))
	{
		if (mask & PART_CLOUD)
		{
			if (part_has(part, PART_CLOUD))
			{
				part_offload(part, false);
				part_unset(part, PART_CLOUD);
			}
		}

		if (mask & PART)
		{
			if (part_has(part, PART))
			{
				part_offload(part, true);
				part_unset(part, PART);
			}
		}

		if (part->state == PART_NONE)
		{
			// unset storage metrics before droping the reference
			auto storage = storage_mgr_find(&self->storage_mgr, &part->source->name);
			storage_remove_metrics(storage, part);

			part->index = NULL;
			buf_reset(&part->index_buf);
		}
	}

	mutex_lock(&self->lock);
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
	if (part->state != PART_NONE)
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

	// wal write
	if (var_int_of(&config()->wal_enable))
	{
		LogDrop drop;
		log_drop_init(&drop);
		drop.id = min;
		auto rotate_ready = wal_write_op(self->wal, &drop.write);

		// schedule wal rotation
		if (rotate_ready)
			service_rotate(self->service);
	}

	return true;
}

void
engine_drop(Engine* self, uint64_t min, bool if_exists, int mask)
{
	// partition drop
	if (mask == (PART|PART_CLOUD))
	{
		for (;;)
		{
			// in order to avoid heavy exclusive lock during drop,
			// separate file drop first
			auto exists = engine_drop_file(self, min, if_exists, false, mask);
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
	engine_drop_file(self, min, if_exists, false, mask);
}

void
engine_drop_range(Engine* self, uint64_t min, uint64_t max, int mask)
{
	uint64_t next;
	for (; engine_foreach(self, &min, &next, max); min = next)
		engine_drop(self, min, true, mask);
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

	// partition exists locally
	if (part_has(part, PART))
	{
		engine_unlock(self, ref, LOCK_SERVICE);
		return;
	}

	// partition does not exists on cloud
	if (! part_has(part, PART_CLOUD))
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

	part_set(part, PART);

	ref_unlock(ref, LOCK_ACCESS);
	mutex_unlock(&self->lock);

	// complete
	engine_unlock(self, ref, LOCK_SERVICE);
}

void
engine_download_range(Engine* self, uint64_t min, uint64_t max, bool if_cloud)
{
	uint64_t next;
	for (; engine_foreach(self, &min, &next, max); min = next)
		engine_download(self, min, true, if_cloud);
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
	if (part_has(part, PART_CLOUD))
	{
		engine_unlock(self, ref, LOCK_SERVICE);
		return;
	}

	// ensure partition file exists locally
	if (! part_has(part, PART))
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

	part_set(part, PART_CLOUD);

	ref_unlock(ref, LOCK_ACCESS);
	mutex_unlock(&self->lock);

	// complete
	engine_unlock(self, ref, LOCK_SERVICE);
}

void
engine_upload_range(Engine* self, uint64_t min, uint64_t max, bool if_cloud)
{
	uint64_t next;
	for (; engine_foreach(self, &min, &next, max); min = next)
		engine_upload(self, min, true, if_cloud);
}

void
engine_refresh(Engine* self, Refresh* refresh, uint64_t min, Str* storage,
               bool if_exists)
{
	// download partition from cloud, if necessary
	engine_download(self, min, if_exists, true);

	// drop partition from cloud
	engine_drop(self, min, if_exists, PART_CLOUD);

	// refresh partition
	refresh_reset(refresh);
	refresh_run(refresh, min, storage, if_exists);

	// upload partition back to cloud
	engine_upload(self, min, true, true);

	// drop partition from storage, only if file been uploaded
	engine_drop_file(self, min, if_exists, true, PART);
}

void
engine_refresh_range(Engine* self, Refresh* refresh, uint64_t min, uint64_t max,
                     Str* storage)
{
	uint64_t next;
	for (; engine_foreach(self, &min, &next, max); min = next)
		engine_refresh(self, refresh, min, storage, true);
}

static inline bool
engine_rebalance_tier_ready(Tier* tier)
{
	// rebalance by partitions
	if (tier->config->partitions >= 0)
		if (tier->storage->list_count > tier->config->partitions)
			return true;

	// rebalance by size
	if (tier->config->size >= 0)
		if (tier->storage->size > tier->config->size)
			return true;

	// rebalance by events
	if (tier->config->events >= 0)
		if (tier->storage->events > tier->config->events)
			return true;

	// rebalance by interval
	if (tier->config->interval >= 0)
		if ((tier->storage->list_count * config_interval()) >
		     (uint64_t)tier->config->interval)
			return true;

	return false;
}

static inline Part*
engine_rebalance_tier(Engine* self, Tier* tier, Str* storage)
{
	// check if tier needs to be rebalanced
	if (! engine_rebalance_tier_ready(tier))
		return NULL;

	// get oldest partition (by psn)
	auto oldest = storage_oldest(tier->storage);

	// schedule partition drop
	if (list_is_last(&self->pipeline.list, &tier->link))
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

	if (pipeline_empty(&self->pipeline))
		return false;

	mutex_lock(&self->lock);

	list_foreach(&self->pipeline.list)
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
			engine_drop(self, min, true, PART|PART_CLOUD);
		else
			engine_refresh(self, refresh, min, &storage, true);
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
		if (!ref->part->refresh && heap_used(&ref->part->memtable->heap) > 0)
		{
			service_refresh(self->service, slice->min);
			ref->part->refresh = true;
		}
		slice = mapping_next(&self->mapping, slice);
	}
}

hot void
engine_gc(Engine* self)
{
	if (! var_int_of(&config()->wal_enable))
		return;

	// take control shared lock
	control_lock_shared();
	guard(lock_guard, control_unlock_guard, NULL);

	// get minimum lsn currently used by any memtable
	uint64_t lsn_min = UINT64_MAX;
	mutex_lock(&self->lock);

	auto slice = mapping_min(&self->mapping);
	while (slice)
	{
		auto ref = ref_of(slice);

		uint64_t min_a = atomic_u64_of(&ref->part->memtable_a.lsn_min);
		uint64_t min_b = atomic_u64_of(&ref->part->memtable_b.lsn_min);

		if (min_a < lsn_min)
			lsn_min = min_a;
		if (min_b < lsn_min)
			lsn_min = min_b;

		slice = mapping_next(&self->mapping, slice);
	}

	mutex_unlock(&self->lock);

	// garbage collect wals < lsn_min
	wal_gc(self->wal, lsn_min);
}

bool
engine_service(Engine* self, Refresh* refresh, bool wait)
{
	ServiceReq* req = NULL;
	auto type = service_next(self->service, wait, &req);
	if (type == SERVICE_NONE)
		return false;
	if (type == SERVICE_SHUTDOWN)
		return true;

	Exception e;
	if (try(&e))
	{
		switch (type) {
		case SERVICE_ROTATE:
		{
			auto wm = var_int_of(&config()->wal_rotate_wm);
			wal_rotate(self->wal, wm);
			break;
		}
		case SERVICE_REBALANCE:
		{
			engine_rebalance(self, refresh);
			break;
		}
		case SERVICE_REFRESH:
		{
			// do rebalance first
			engine_rebalance(self, refresh);
			if (type == SERVICE_REFRESH)
				engine_refresh(self, refresh, req->min, NULL, true);
			break;
		}
		default:
			break;
		}

		// garbage collect wals
		engine_gc(self);
	}
	if (catch(&e))
	{ }

	if (req)
		service_req_free(req);

	return false;
}
