
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
engine_init(Engine* self, Comparator* comparator, Service* service)
{
	self->service    = service;
	self->comparator = comparator;
	rwlock_init(&self->lock_global);
	mutex_init(&self->lock);
	cond_var_init(&self->cond_var);
	mapping_init(&self->mapping, comparator);
	storage_mgr_init(&self->storage_mgr);
	conveyor_init(&self->conveyor, &self->storage_mgr);
}

void
engine_free(Engine* self)
{
	rwlock_free(&self->lock_global);
	mutex_free(&self->lock);
	cond_var_free(&self->cond_var);
	conveyor_free(&self->conveyor);
	storage_mgr_free(&self->storage_mgr);
}

void
engine_open(Engine* self)
{
	// recover storages
	storage_mgr_open(&self->storage_mgr);

	// recover conveyor
	conveyor_open(&self->conveyor);

	// recover partitions
	engine_recover(self);
}

void
engine_close(Engine* self)
{
	storage_mgr_close(&self->storage_mgr);
}

void
engine_lock_global(Engine* self, bool shared)
{
	rwlock_lock(&self->lock_global, shared);
}

void
engine_unlock_global(Engine* self)
{
	rwlock_unlock(&self->lock_global);
}

static Slice*
engine_create(Engine* self, uint64_t min, uint64_t max)
{
	// validate storage manager and conveyor
	conveyor_validate(&self->conveyor);

	auto head = mapping_max(&self->mapping);

	// create new reference
	auto ref  = ref_allocate(min, max);
	guard(guard, ref_free, ref);

	// create new partition
	auto id   = config_psn_next();
	auto part = part_allocate(self->comparator, NULL, id, id, min, max);
	ref_prepare(ref, &self->lock, &self->cond_var, part);

	// update mapping
	mapping_add(&self->mapping, &ref->slice);

	// match primary storage
	Storage* storage = NULL;
	Tier*    primary = conveyor_primary(&self->conveyor);
	if (primary)
	{
		// first storage according to the conveyor order
		storage = primary->storage;
	} else
	{
		// conveyor is not set, using first storage
		assert(self->storage_mgr.list_count == 1);
		storage = storage_mgr_first(&self->storage_mgr);
	}
	storage_add(storage, part);
	part->target = storage->target;

	// schedule rebalance
	if (! conveyor_empty(&self->conveyor))
		service_rebalance(self->service);

	// schedule refresh for previous head
	if (head && head->min < min)
		service_refresh(self->service, head->min);

	unguard(&guard);
	return &ref->slice;
}

hot Ref*
engine_lock(Engine* self, uint64_t min, RefLock lock,
            bool gte,
            bool create_if_not_exists)
{
	// take shared engine lock to avoid exclusive operations
	engine_lock_global(self, true);
	guard(cglguard, engine_unlock_global, self);

	mutex_lock(&self->lock);
	guard(unlock, mutex_unlock, &self->lock);

	Slice* slice;
	if (gte)
	{
		// engine is empty
		if (unlikely(self->mapping.tree_count == 0))
			return NULL;

		// find partition >= min
		slice = mapping_seek(&self->mapping, min);
		if (slice->max <= min)
			return NULL;
	} else
	{
		// find partition = min
		slice = mapping_match(&self->mapping, min);
		if (! slice)
		{
			if (! create_if_not_exists)
				return NULL;
			auto max = min + config_interval();
			slice = engine_create(self, min, max);
		}
	}

	// take the reference locks
	auto ref = ref_of(slice);
	ref_lock(ref, lock);

	// shared engine lock is kept until the reference
	// is unlocked
	unguard(&cglguard);
	return ref;
}

void
engine_unlock(Engine* self, Ref* ref, RefLock lock)
{
	ref_unlock(ref, lock);

	// unlock or dereference global engine lock
	engine_unlock_global(self);
}
