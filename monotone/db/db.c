
//
// monotone
//
// time-series storage
//

#include <monotone_runtime.h>
#include <monotone_lib.h>
#include <monotone_io.h>
#include <monotone_storage.h>
#include <monotone_db.h>

void
db_init(Db* self, Comparator* comparator)
{
	self->rows_written       = 0;
	self->rows_written_bytes = 0;
	self->comparator         = comparator;
	mutex_init(&self->lock);
	part_tree_init(&self->tree, comparator);
	storage_mgr_init(&self->storage_mgr);
	conveyor_init(&self->conveyor, &self->storage_mgr);
	lock_mgr_init(&self->lock_mgr);
}

void
db_free(Db* self)
{
	mutex_free(&self->lock);
	lock_mgr_free(&self->lock_mgr);
	conveyor_free(&self->conveyor);
	storage_mgr_free(&self->storage_mgr);
}

void
db_open(Db* self)
{
	// recover storages objects
	storage_mgr_open(&self->storage_mgr);

	// recover conveyor
	conveyor_open(&self->conveyor);

	// recover partitions
	db_recover(self);
}

void
db_close(Db* self)
{
	auto part = part_tree_min(&self->tree);
	while (part)
	{
		auto next = part_tree_next(&self->tree, part);
		part_free(part);
		part = next;
	}
}

#if 0
static inline Part*
db_create(Db* self, uint64_t time)
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
db_find(Db* self, bool create, uint64_t time)
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
	part = db_create(self, time);
	lock->arg = part;
	return lock;
}

hot Lock*
db_seek(Db* self, uint64_t time)
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
#endif
