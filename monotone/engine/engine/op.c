
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

static Slice*
engine_create(Engine* self, uint64_t min, uint64_t max)
{
	// create new reference
	auto ref = ref_allocate(min, max);
	guard(guard, ref_free, ref);

	// create new partition
	Id id =
	{
		.min = min,
		.max = max
	};
	auto part = part_allocate(self->comparator, NULL, &id);
	part_set_time(part, time_us());
	ref_prepare(ref, &self->lock, &self->cond_var, part);

	// update mapping
	mapping_add(&self->mapping, &ref->slice);

	// match primary storage
	Storage* storage = NULL;
	Tier*    primary = pipeline_primary(&self->pipeline);
	if (primary)
	{
		// first storage according to the pipeline order
		storage = primary->storage;
	} else
	{
		// pipeline is not set, using main storage
		storage = storage_mgr_first(&self->storage_mgr);
	}
	storage_add(storage, part);
	part->source = storage->source;
	unguard(&guard);

	// schedule rebalance
	if (! pipeline_empty(&self->pipeline))
		service_rebalance(self->service);

	return &ref->slice;
}

void
engine_fill(Engine* self, uint64_t min, uint64_t max, bool lock)
{
	if (lock)
	{
		// take shared control lock to avoid exclusive operations
		control_lock_shared();
		guard(cglguard, control_unlock_guard, NULL);

		mutex_lock(&self->lock);
		guard(unlock, mutex_unlock, &self->lock);
	}

	// validate range
	if (unlikely(min > max))
		error("fill: invalid partition interval");

	// empty
	if (unlikely(self->mapping.tree_count == 0))
	{
		engine_create(self, min, max);
		return;
	}

	// create one or more partitions to fill the gap between
	// min and max range
	Buf gaps;
	buf_init(&gaps);
	guard(guard, buf_free, &gaps);

	auto slice = mapping_seek(&self->mapping, min);
	while (min <= max)
	{
		if (slice)
		{
			//       [....] slice
			//  [....]
			if (min < slice->min)
			{
				Id gap;
				gap.min = min;
				if (max < slice->min)
					gap.max = max;
				else
					gap.max = slice->min - 1;
				buf_write(&gaps, &gap, sizeof(gap));

				min = slice->max + 1;
			} else
			if (slice_in(slice, min))
			{
				min = slice->max + 1;
			}

			slice = mapping_next(&self->mapping, slice);
			continue;
		}

		Id gap =
		{
			.min = min,
			.max = max
		};
		buf_write(&gaps, &gap, sizeof(gap));
		break;
	}

	// create partitions using gaps
	auto pos = (Id*)gaps.start;
	auto end = (Id*)gaps.position;
	for (; pos < end; pos++)
		engine_create(self, pos->min, pos->max);
}

static inline bool
engine_foreach(Engine* self, uint64_t* min, uint64_t* min_next, uint64_t max)
{
	// get next reference >= min and <= max
	auto ref = engine_lock(self, *min, LOCK_ACCESS, true, false);
	if (ref == NULL)
		return false;
	uint64_t ref_min = ref->slice.min;
	uint64_t ref_max = ref->slice.max;
	engine_unlock(self, ref, LOCK_ACCESS);
	if (ref_min >= *min && ref_max <= max)
	{
		*min = ref_min;
		*min_next = ref_max + 1;
		return true;
	}
	return false;
}

static bool
engine_drop_file(Engine* self, uint64_t min, bool if_exists,
                 bool    cloud_drop_local,
                 int     mask)
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

	// execute automatic drop only, if file is on cloud and allowed
	// by the partition source
	if (cloud_drop_local)
	{
		if (!part->source->cloud_drop_local || !part_has(part, ID_CLOUD))
		{
			engine_unlock(self, ref, LOCK_SERVICE);
			return false;
		}
	}

	// get access lock
	mutex_lock(&self->lock);
	ref_lock(ref, LOCK_ACCESS);
	mutex_unlock(&self->lock);

	// delete partition file on storage or cloud
	Exception e;
	if (try(&e))
	{
		if (mask & ID_CLOUD)
		{
			if (part_has(part, ID_CLOUD))
			{
				part_offload(part, false);
				part_unset(part, ID_CLOUD);
			}
		}

		if (mask & ID)
		{
			if (part_has(part, ID))
			{
				part_offload(part, true);
				part_unset(part, ID);
			}
		}

		if (part->state == ID_NONE)
		{
			// unset storage metrics before droping the reference
			auto storage = storage_mgr_find(&self->storage_mgr, &part->source->name);
			storage_remove_metrics(storage, part);
			index_init(&part->index);
			buf_reset(&part->index_data);
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
	if (part->state != ID_NONE)
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
	if (mask == (ID|ID_CLOUD))
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
	if (part_has(part, ID))
	{
		engine_unlock(self, ref, LOCK_SERVICE);
		return;
	}

	// partition does not exists on cloud
	if (! part_has(part, ID_CLOUD))
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

	part_set(part, ID);

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
	if (part_has(part, ID_CLOUD))
	{
		engine_unlock(self, ref, LOCK_SERVICE);
		return;
	}

	// ensure partition file exists locally
	if (! part_has(part, ID))
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

	part_set(part, ID_CLOUD);

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
	engine_drop(self, min, if_exists, ID_CLOUD);

	// refresh partition
	refresh_reset(refresh);
	refresh_run(refresh, min, storage, if_exists);

	// upload partition back to cloud
	engine_upload(self, min, true, true);

	// drop partition from storage, only if file been uploaded
	engine_drop_file(self, min, if_exists, true, ID);
}

void
engine_refresh_range(Engine* self, Refresh* refresh, uint64_t min, uint64_t max,
                     Str* storage)
{
	uint64_t next;
	for (; engine_foreach(self, &min, &next, max); min = next)
		engine_refresh(self, refresh, min, storage, true);
}

static inline Part*
engine_rebalance_tier_next(Tier* tier)
{
	// rebalance by partitions
	if (tier->config->partitions >= 0)
		if (tier->storage->list_count > tier->config->partitions)
			return storage_oldest(tier->storage);

	// rebalance by size
	if (tier->config->size >= 0)
		if (tier->storage->size > tier->config->size)
			return storage_oldest(tier->storage);

	// rebalance by events
	if (tier->config->events >= 0)
		if (tier->storage->events > tier->config->events)
			return storage_oldest(tier->storage);

	// rebalance by duration
	if (tier->config->duration >= 0)
	{
		auto part = storage_oldest(tier->storage);
		if (! part)
			return NULL;
		auto now = time_us();
		if ((now - part->time) >= (uint64_t)tier->config->duration)
			return part;
	}

	return NULL;
}

static inline Part*
engine_rebalance_tier(Engine* self, Tier* tier, Str* storage)
{
	// check if tier needs to be rebalanced, get next partition
	auto part = engine_rebalance_tier_next(tier);
	if (! part)
		return NULL;

	// schedule partition drop
	if (list_is_last(&self->pipeline.list, &tier->link))
		return part;

	// schedule partition move to the next tier storage
	auto next = container_of(tier->link.next, Tier, link);
	str_copy(storage, &next->storage->source->name);
	return part;
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
			engine_drop(self, min, true, ID|ID_CLOUD);
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
			auto part = ref->part;
			service_refresh(self->service, part);
			part->refresh = true;
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
engine_service(Engine* self, Refresh* refresh, ServiceFilter filter, bool wait)
{
	ServiceReq* req = NULL;
	bool shutdown = service_next(self->service, filter, wait, &req);
	if (shutdown)
		return true;
	if (req == NULL)
		return false;

	Exception e;
	if (try(&e))
	{
		while (req->current < req->actions_count)
		{
			// if current action does not apply to current worker type,
			// reschedule request
			if (! service_filter(req, filter))
			{
				service_schedule(self->service, req);
				req = NULL;
				break;
			}

			auto action = &req->actions[req->current];
			switch (action->type) {
			case ACTION_ROTATE:
			{
				auto wm = var_int_of(&config()->wal_rotate_wm);
				wal_rotate(self->wal, wm);
				break;
			}
			case ACTION_GC:
				engine_gc(self);
				break;
			case ACTION_REBALANCE:
				engine_rebalance(self, refresh);
				break;
			case ACTION_DROP_ON_STORAGE:
				// drop partition file from storage, only if the file
				// has been uploaded
				engine_drop_file(self, req->id, true, true, ID);
				break;
			case ACTION_DROP_ON_CLOUD:
				engine_drop(self, req->id, true, ID_CLOUD);
				break;
			case ACTION_REFRESH:
				refresh_reset(refresh);
				refresh_run(refresh, req->id, NULL, true);
				break;
			case ACTION_DOWNLOAD:
				engine_download(self, req->id, true, true);
				break;
			case ACTION_UPLOAD:
				engine_upload(self, req->id, true, true);
				break;
			default:
				break;
			}
			req->current++;
		}
	}
	if (catch(&e))
	{ }

	if (req)
		service_req_free(req);

	return false;
}

void
engine_resume(Engine* self)
{
	// resume operations after restart

	// take exclusive control lock
	control_lock_exclusive();
	guard(guard, control_unlock_guard, NULL);

	// resume upload
	auto slice = mapping_min(&self->mapping);
	while (slice)
	{
		auto ref  = ref_of(slice);
		auto part = ref->part;
		if (part->cloud && !part_has(part, ID_CLOUD))
			service_upload(self->service, part);

		slice = mapping_next(&self->mapping, slice);
	}
}
