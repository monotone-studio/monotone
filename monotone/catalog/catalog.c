
//
// monotone
//
// time-series storage
//

#include <monotone_runtime.h>
#include <monotone_lib.h>
#include <monotone_io.h>
#include <monotone_catalog.h>

void
catalog_init(Catalog* self, Comparator* comparator, Service* service)
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
catalog_free(Catalog* self)
{
	rwlock_free(&self->lock_global);
	mutex_free(&self->lock);
	cond_var_free(&self->cond_var);
	conveyor_free(&self->conveyor);
	storage_mgr_free(&self->storage_mgr);
}

void
catalog_open(Catalog* self)
{
	// recover storages
	storage_mgr_open(&self->storage_mgr);

	// recover conveyor
	conveyor_open(&self->conveyor);
}

void
catalog_close(Catalog* self)
{
	storage_mgr_close(&self->storage_mgr);
}

void
catalog_lock_global(Catalog* self, bool shared)
{
	rwlock_lock(&self->lock_global, shared);
}

void
catalog_unlock_global(Catalog* self)
{
	rwlock_unlock(&self->lock_global);
}

static Slice*
catalog_create(Catalog* self, uint64_t min, uint64_t max)
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

	return unguard(&guard);
}

hot Ref*
catalog_lock(Catalog* self, uint64_t min, int lock,
             bool gte,
             bool create_if_not_exists)
{
	// take shared catalog lock to avoid exclusive operations
	catalog_lock_global(self, true);
	guard(cglguard, catalog_unlock_global, self);

	mutex_lock(&self->lock);
	guard(unlock, mutex_unlock, &self->lock);

	Slice* slice;
	if (gte)
	{
		// catalog is empty
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
			slice = catalog_create(self, min, max);
		}
	}

	// take the reference locks
	auto ref = ref_of(slice);
	ref_lock(ref, lock);

	// shared catalog lock is kept until the reference
	// is unlocked
	unguard(&cglguard);
	return ref;
}

void
catalog_unlock(Catalog* self, Ref* ref, int lock)
{
	ref_unlock(ref, lock);

	// unlock or dereference global catalog lock
	catalog_unlock_global(self);
}

void
catalog_drop(Catalog* self, uint64_t min, bool if_exists)
{
	// take exclusive catalog lock
	catalog_lock_global(self, false);

	// find partition = min
	auto slice = mapping_match(&self->mapping, min);
	if (! slice)
	{
		catalog_unlock_global(self);
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

	catalog_unlock_global(self);

	// delete partition file and free
	part_delete(ref->part, true);
	ref_free(ref);
}
