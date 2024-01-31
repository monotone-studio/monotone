
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
engine_drop(Engine* self, uint64_t min, bool if_exists)
{
	// take exclusive control lock
	control_lock_exclusive();

	// find partition = min
	auto slice = mapping_match(&self->mapping, min);
	if (! slice)
	{
		control_unlock();
		if (! if_exists)
			error("drop: partition <%" PRIu64 "> does not exists", min);
		return;
	}

	// remove partition from mapping
	mapping_remove(&self->mapping, slice);

	// remove partition from storage
	auto ref = ref_of(slice);
	auto storage = storage_mgr_find(&self->storage_mgr, &ref->part->source->name);
	storage_remove(storage, ref->part);

	control_unlock();

	// delete partition file and free
	part_delete(ref->part, true);
	ref_free(ref);
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

void
engine_drop_range(Engine* self, uint64_t min, uint64_t max)
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

		engine_drop(self, ref_min, true);
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
			engine_drop(self, min, true);
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
