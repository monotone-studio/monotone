
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

void
engine_init(Engine* self, Comparator* comparator, Service* service)
{
	self->service    = service;
	self->comparator = comparator;
	mutex_init(&self->lock);
	cond_var_init(&self->cond_var);
	mapping_init(&self->mapping, comparator);
	storage_mgr_init(&self->storage_mgr);
	conveyor_init(&self->conveyor, &self->storage_mgr);
}

void
engine_free(Engine* self)
{
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

static Slice*
engine_create(Engine* self, uint64_t min, uint64_t max)
{
	auto head = mapping_max(&self->mapping);

	// create new reference
	auto ref  = ref_allocate(min, max);
	guard(guard, ref_free, ref);

	// create new partition
	auto psn  = config_psn_next();
	PartId id =
	{
		.id        = psn,
		.id_parent = psn,
		.min       = min,
		.max       = max
	};
	auto part = part_allocate(self->comparator, NULL, &id);
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
		// conveyor is not set, using main storage
		assert(self->storage_mgr.list_count == 1);
		storage = storage_mgr_first(&self->storage_mgr);
	}
	storage_add(storage, part);
	part->source = storage->source;
	unguard(&guard);

	// schedule rebalance
	if (! conveyor_empty(&self->conveyor))
		service_rebalance(self->service);

	// schedule refresh for previous head
	if (head && head->min < min)
		service_refresh(self->service, head->min);

	return &ref->slice;
}

hot Ref*
engine_lock(Engine* self, uint64_t min, LockType lock,
            bool gte,
            bool create_if_not_exists)
{
	// take shared control lock to avoid exclusive operations
	control_lock_shared();
	guard(cglguard, control_unlock_guard, NULL);

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
engine_unlock(Engine* self, Ref* ref, LockType lock)
{
	mutex_lock(&self->lock);
	guard(unlock, mutex_unlock, &self->lock);

	ref_unlock(ref, lock);

	// unlock or dereference shared control lock
	control_unlock();
}
