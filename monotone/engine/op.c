
//
// monotone
//
// time-series storage
//

#include <monotone_runtime.h>
#include <monotone_lib.h>
#include <monotone_io.h>
#include <monotone_catalog.h>
#include <monotone_engine.h>

void
engine_refresh(Engine* self, Compaction* cp, uint64_t min,
               bool if_exists)
{
	unused(self);
	compaction_reset(cp);
	compaction_run(cp, min, NULL, if_exists);
}

void
engine_move(Engine* self, Compaction* cp, uint64_t min, Str* storage,
            bool if_exists)
{
	unused(self);
	compaction_reset(cp);
	compaction_run(cp, min, storage, if_exists);
}

void
engine_drop(Engine* self, uint64_t min, bool if_exists)
{
	// take exclusive engine lock
	engine_lock_global(self, false);

	// find partition = min
	auto slice = mapping_match(&self->mapping, min);
	if (! slice)
	{
		engine_unlock_global(self);
		if (! if_exists)
			error("drop: partition <%" PRIu64 "> does not exists", min);
		return;
	}

	// remove partition from mapping
	mapping_remove(&self->mapping, slice);

	// remove partition from storage
	auto ref = ref_of(slice);
	auto storage = storage_mgr_find(&self->storage_mgr, &ref->part->target->name);
	storage_remove(storage, ref->part);

	engine_unlock_global(self);

	// delete partition file and free
	part_delete(ref->part, true);
	ref_free(ref);
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
	str_copy(storage, &next->storage->target->name);
	return oldest;
}

static bool
engine_rebalance_next(Engine* self, uint64_t* min, Str* storage)
{
	// take engine shared lock
	engine_lock_global(self, true);
	guard(lock_guard, engine_unlock_global, self);

	if (conveyor_empty(&self->conveyor))
		return false;

	mutex_lock(&self->lock);

	list_foreach(&self->conveyor.list)
	{
		auto tier = list_at(Tier, link);
		auto part = engine_rebalance_tier(self, tier, storage);
		if (part)
		{
			*min = part->min;
			mutex_unlock(&self->lock);
			return true;
		}
	}

	mutex_unlock(&self->lock);
	return false;
}

void
engine_rebalance(Engine* self, Compaction* cp)
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
			engine_move(self, cp, min, &storage, true);
	}
}

void
engine_checkpoint(Engine* self)
{
	// take exclusive lock
	engine_lock_global(self, false);
	guard(guard, engine_unlock_global, self);

	// schedule refresh
	auto slice = mapping_min(&self->mapping);
	while (slice)
	{
		auto ref = ref_of(slice);
		if (!ref->refresh && ref->part->memtable->size > 0)
		{
			service_refresh(self->service, slice->min);
			ref->refresh = true;
		}
		slice = mapping_next(&self->mapping, slice);
	}
}
