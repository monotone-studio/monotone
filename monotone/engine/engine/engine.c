
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

void
engine_init(Engine*     self,
            Comparator* comparator,
            Wal*        wal,
            Service*    service,
            CloudMgr*   cloud_mgr)
{
	self->wal        = wal;
	self->service    = service;
	self->comparator = comparator;
	mutex_init(&self->lock);
	cond_var_init(&self->cond_var);
	mapping_init(&self->mapping);
	storage_mgr_init(&self->storage_mgr, cloud_mgr);
	pipeline_init(&self->pipeline, &self->storage_mgr);
}

void
engine_free(Engine* self)
{
	mutex_free(&self->lock);
	cond_var_free(&self->cond_var);
	pipeline_free(&self->pipeline);
	storage_mgr_free(&self->storage_mgr);
}

void
engine_open(Engine* self)
{
	// recover storages
	storage_mgr_open(&self->storage_mgr);
	storage_mgr_create_main(&self->storage_mgr);

	// recover pipeline
	pipeline_open(&self->pipeline);

	// recover partitions
	engine_recover(self);
}

void
engine_close(Engine* self)
{
	for (;;)
	{
		auto slice = mapping_min(&self->mapping);
		if (! slice)
			break;
		mapping_remove(&self->mapping, slice);
		auto ref = ref_of(slice);
		ref_free(ref);
	}
}

void
engine_set_serial(Engine* self)
{
	auto slice = mapping_max(&self->mapping);
	if (! slice)
		return;
	auto ref  = ref_of(slice);
	auto part = ref->part;

	bool     serial_set = false;
	uint64_t serial = 0;
	if (part->state != ID_NONE && part->index.regions > 0)
	{
		serial = index_max(&part->index, &part->index_data);
		serial_set = true;
	}

	auto max = memtable_max(part->memtable);
	if (max && max->id >= serial)
	{
		serial = max->id;
		serial_set = true;
	}

	if (serial_set)
		serial++;

	config_ssn_set(serial);
}

hot Ref*
engine_lock(Engine* self, uint64_t id, LockType lock,
            bool    gte,
            bool    create_if_not_exists)
{
	// take shared control lock to avoid exclusive operations
	control_lock_shared();
	guard_as(cglguard, control_unlock_guard, NULL);

	mutex_lock(&self->lock);
	guard(mutex_unlock, &self->lock);

	Slice* slice;
	if (gte)
	{
		// find partition min >= id
		slice = mapping_gte(&self->mapping, id);
		if (! slice)
			return NULL;
	} else
	{
		// find partition = min
		auto head = mapping_max(&self->mapping);
		if (likely(head && slice_in(head, id)))
			slice = head;
		else
			slice = mapping_match(&self->mapping, id);
		if (! slice)
		{
			if (! create_if_not_exists)
				return NULL;

			// create one or more partitions using default interval

			// interval is inclusive
			auto min = config_interval_of(id);
			auto max = (min + config_interval()) - 1;
			engine_fill(self, min, max, false);

			// retry search
			slice = mapping_match(&self->mapping, id);
			assert(slice);

			// schedule refresh for previous head
			if (head && head->min < min)
				service_refresh(self->service, ref_of(head)->part);
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
	guard(mutex_unlock, &self->lock);

	ref_unlock(ref, lock);

	// unlock or dereference shared control lock
	control_unlock();
}
