
//
// noire
//
// time-series storage
//

#include <noire_runtime.h>
#include <noire_lib.h>
#include <noire_io.h>
#include <noire_engine.h>
#include <noire_instance.h>

void
instance_init(Instance* self)
{
	self->global.config = &self->config;
	service_init(&self->service);
	comparator_init(&self->comparator);
	storage_mgr_init(&self->storage_mgr);
	engine_init(&self->engine, &self->comparator,
	            &self->service,
	            &self->storage_mgr);
	compaction_mgr_init(&self->compaction_mgr);
	config_init(&self->config);
}

void
instance_free(Instance* self)
{
	engine_free(&self->engine);
	service_free(&self->service);
	storage_mgr_free(&self->storage_mgr);
	config_free(&self->config);
}

void
instance_start(Instance* self)
{
	// create default storage
	if (self->storage_mgr.list_count == 0)
	{
		Str name;
		str_set_cstr(&name, "default");
		auto storage = storage_allocate(&name);
		storage_mgr_add(&self->storage_mgr, storage);
	}

	// recover instance engine
	engine_open(&self->engine);

	// start compaction
	compaction_mgr_start(&self->compaction_mgr, &self->service, &self->engine);
}

void
instance_stop(Instance* self)
{
	// shutdown service
	service_shutdown(&self->service);

	// stop compaction
	compaction_mgr_stop(&self->compaction_mgr);

	// shutdown engine
	engine_close(&self->engine);
}

hot void
instance_insert(Instance* self, uint64_t time, void* data, int data_size)
{
	// get min/max range
	uint64_t min = time - (time % config()->interval);
	uint64_t max = min + config()->interval;

	// allocate row
	auto row = memtable_row_allocate(time, data, data_size);

	// find or create partition
	auto lock = engine_find(&self->engine, true, min);
	guard(unlock, lock_mgr_unlock, lock);
	Part* part = lock->arg;

	// update memtable
	auto memtable = part->memtable;
	memtable_set(memtable, row);

	// todo: wal write
	memtable_follow_lsn(memtable, 0);

	// schedule compaction
	if (part->storage->compaction_wm > 0)
	{
		if (part->memtable->size > (uint32_t)part->storage->compaction_wm)
		{
			if (! part->service)
			{
				part->service = true;
				service_add(&self->service, min, max);
			}
		}
	}
}
