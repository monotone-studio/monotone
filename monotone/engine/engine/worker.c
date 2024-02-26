
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

static void*
worker_main(void* arg)
{
	Worker* self = arg;
	runtime_init(self->context);
	thread_set_name(&self->thread, "worker");

	for (;;)
	{
		bool shutdown = engine_service(self->engine, &self->refresh, true);
		if (shutdown)
			break;
	}

	return NULL;
}

void
worker_init(Worker* self, Engine* engine)
{
	self->engine  = engine;
	self->context = NULL;
	refresh_init(&self->refresh, engine);
	thread_init(&self->thread);
}

void
worker_free(Worker* self)
{
	refresh_free(&self->refresh);
}

void
worker_start(Worker* self)
{
	self->context = mn_runtime.context;
	int rc;
	rc = thread_create(&self->thread, worker_main, self);
	if (unlikely(rc == -1))
		error_system();
}

void
worker_stop(Worker* self)
{
	thread_join(&self->thread);
}
