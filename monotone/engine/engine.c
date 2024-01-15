
//
// monotone
//
// time-series storage
//

#include <monotone_runtime.h>
#include <monotone_lib.h>
#include <monotone_io.h>
#include <monotone_storage.h>
#include <monotone_engine.h>

void
engine_init(Engine* self, Comparator* comparator)
{
	self->comparator = comparator;
	mutex_init(&self->lock);
	part_tree_init(&self->tree, comparator);
	storage_mgr_init(&self->storage_mgr);
	conveyor_init(&self->conveyor, &self->storage_mgr);
	lock_mgr_init(&self->lock_mgr);
}

void
engine_free(Engine* self)
{
	mutex_free(&self->lock);
	lock_mgr_free(&self->lock_mgr);
	conveyor_free(&self->conveyor);
	storage_mgr_free(&self->storage_mgr);
}

void
engine_open(Engine* self)
{
	// recover storages objects
	storage_mgr_open(&self->storage_mgr);

	// recover conveyor
	conveyor_open(&self->conveyor);

	// recover partitions
	engine_recover(self);
}

void
engine_close(Engine* self)
{
	auto part = part_tree_min(&self->tree);
	while (part)
	{
		auto next = part_tree_next(&self->tree, part);
		part_free(part);
		part = next;
	}
}

static inline Part*
engine_create(Engine* self, uint64_t min)
{
	auto head = part_tree_max(&self->tree);

	// create new partition
	auto id = config_psn_next();
	auto part = part_allocate(self->comparator, NULL, id, id,
	                          min,
	                          min + config_interval());
	part_tree_add(&self->tree, part);

	// match primary storage
	Tier* primary = conveyor_primary(&self->conveyor);
	auto  storage = primary->storage;
	storage_add(storage, part);
	part->target = storage->target;

	/*
	// rebalance partitions
	if (self->conveyor.list_count > 1)
	{
		if (storage->list_count >= primary->config->capacity)
			service_add(&self->service, 0, 0);
	}
	*/

	// schedule former max compaction
	if (head && head->min < min)
		service_add(&self->service, SERVICE_MERGE, head->min, NULL);

	return part;
}

hot Lock*
engine_find(Engine* self, bool create, uint64_t min)
{
	// lock partition by min
	auto lock = lock_mgr_get(&self->lock_mgr, min);
	guard(guard, lock_mgr_unlock, lock);

	mutex_lock(&self->lock);
	guard(unlock, mutex_unlock, &self->lock);

	// validate storage manager and conveyor
	conveyor_validate(&self->conveyor);

	// find partition by min
	lock->part = part_tree_match(&self->tree, min);
	if (lock->part)
		return unguard(&guard);
	if (! create)
		return NULL;

	// create new partition
	lock->part = engine_create(self, min);
	return unguard(&guard);
}

hot Lock*
engine_seek(Engine* self, uint64_t min)
{
	// try to find a partition and grab a lock with min time
	for (;;)
	{
		auto lock = lock_mgr_get(&self->lock_mgr, min);
		mutex_lock(&self->lock);

		if (unlikely(self->tree.tree_count == 0))
		{
			mutex_unlock(&self->lock);
			lock_mgr_unlock(lock);
			break;
		}

		auto part = part_tree_seek(&self->tree, min);
		if (part->max <= min)
		{
			mutex_unlock(&self->lock);
			lock_mgr_unlock(lock);
			break;
		}

		if (part->min == min)
		{
			lock->part = part;
			mutex_unlock(&self->lock);
			return lock;
		}

		min = part->min;
		mutex_unlock(&self->lock);
		lock_mgr_unlock(lock);
	}

	return NULL;
}
