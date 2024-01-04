
//
// monotone
//
// time-series storage
//

#include <monotone_runtime.h>
#include <monotone_lib.h>
#include <monotone_io.h>
#include <monotone_engine.h>
#include <monotone_instance.h>

void
instance_init(Instance* self)
{
	self->global.config      = &self->config;
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
instance_start(Instance* self, const char* options)
{
	// read configuration
	instance_config(self, options);

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
