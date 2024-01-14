
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

static inline Part*
db_create(Db* self, uint64_t min)
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
db_find(Db* self, bool create, uint64_t min)
{
	// lock partition by min
	auto lock = lock_mgr_get(&self->lock_mgr, min);

	mutex_lock(&self->lock);
	guard(unlock, mutex_unlock, &self->lock);

	// find partition by min
	auto part = part_tree_match(&self->tree, min);
	if (part)
	{
		lock->arg = part;
		return lock;
	}
	if (! create)
		return NULL;

	// create new partition
	part = db_create(self, min);
	lock->arg = part;
	return lock;
}

hot Lock*
db_seek(Db* self, uint64_t min)
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
			lock->arg = part;
			mutex_unlock(&self->lock);
			return lock;
		}

		min = part->min;
		mutex_unlock(&self->lock);
		lock_mgr_unlock(lock);
	}

	return NULL;
}
