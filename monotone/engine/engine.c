
//
// monotone
//
// time-series storage
//

#include <monotone_runtime.h>
#include <monotone_lib.h>
#include <monotone_io.h>
#include <monotone_engine.h>

void
engine_init(Engine*     self,
            Comparator* comparator,
            Service*    service,
            StorageMgr* storage_mgr)
{
	self->list_count         = 0;
	self->rows_written       = 0;
	self->rows_written_bytes = 0;
	self->service            = service;
	self->comparator         = comparator;
	mutex_init(&self->lock);
	list_init(&self->list);
	part_tree_init(&self->tree, comparator);
	tier_mgr_init(&self->tier_mgr, storage_mgr);
	lock_mgr_init(&self->lock_mgr);
}

void
engine_free(Engine* self)
{
	assert(! self->list_count);
	lock_mgr_free(&self->lock_mgr);
	mutex_free(&self->lock);
	tier_mgr_free(&self->tier_mgr);
}

void
engine_open(Engine* self)
{
	// prepare tier manager
	tier_mgr_create(&self->tier_mgr);

	engine_recover(self);
}

void
engine_close(Engine* self)
{
	list_foreach_safe(&self->list)
	{
		auto part = list_at(Part, link);
		part_free(part);
	}
	self->list_count = 0;
	list_init(&self->list);
}

void
engine_flush(Engine* self)
{
	mutex_lock(&self->lock);
	guard(unlock, mutex_unlock, &self->lock);

	list_foreach(&self->list)
	{
		auto part = list_at(Part, link);
		if (part->service)
			continue;
		if (part->memtable->size > 0)
		{
			part->service = true;
			service_add(self->service, part->min, part->max);
		}
	}
}

static inline Part*
engine_create(Engine* self, uint64_t time)
{
	auto head = part_tree_max(&self->tree);

	// create new partition
	auto part = part_allocate(self->comparator, NULL,
	                          time,
	                          time + config()->interval);
	list_append(&self->list, &part->link);
	self->list_count++;

	part_tree_add(&self->tree, part);

	// match tier
	int tier_order = 0;
	if (self->tier_mgr.tiers_count > 1)
	{
		auto ref = part_tree_prev(&self->tree, part);
		if (ref) {
			tier_order = ref->storage->order;
		} else
		{
			ref = part_tree_next(&self->tree, part);
			if (ref)
				tier_order = ref->storage->order;
		}
	}
	auto tier = tier_of(&self->tier_mgr, tier_order);
	tier_add(tier, part);
	part->storage = tier->storage;

	// rebalance partitions
	if (self->tier_mgr.tiers_count > 1)
	{
		auto storage = tier->storage;
		if (storage->capacity != INT_MAX && tier->list_count >= storage->capacity)
			service_add(self->service, 0, 0);
	}

	// schedule former max compaction
	if (head && head->min < time)
	{
		if (! head->service)
		{
			head->service = true;
			service_add(self->service, head->min, head->max);
		}
	}

	return part;
}

hot Lock*
engine_find(Engine* self, bool create, uint64_t time)
{
	// lock partition by min
	auto lock = lock_mgr_get(&self->lock_mgr, time);

	mutex_lock(&self->lock);
	guard(unlock, mutex_unlock, &self->lock);

	// find partition by min
	Part* part = NULL;
	if (likely(self->list_count > 0))
	{
		part = part_tree_match(&self->tree, time);
		if (part)
		{
			lock->arg = part;
			return lock;
		}
	}
	if (! create)
		return NULL;

	// create new partition
	part = engine_create(self, time);
	lock->arg = part;
	return lock;
}

hot Lock*
engine_seek(Engine* self, uint64_t time)
{
	// try to find a partition and grab a lock with min time
	for (;;)
	{
		auto lock = lock_mgr_get(&self->lock_mgr, time);
		mutex_lock(&self->lock);

		if (unlikely(self->list_count == 0))
		{
			mutex_unlock(&self->lock);
			lock_mgr_unlock(lock);
			break;
		}

		auto part = part_tree_seek(&self->tree, time);
		if (part->max <= time)
		{
			mutex_unlock(&self->lock);
			lock_mgr_unlock(lock);
			break;
		}

		if (part->min == time)
		{
			lock->arg = part;
			mutex_unlock(&self->lock);
			return lock;
		}

		time = part->min;
		mutex_unlock(&self->lock);
		lock_mgr_unlock(lock);
	}
	return NULL;
}
