
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

static void*
worker_main(void* arg)
{
	Worker* self = arg;
	runtime_init(self->context);
	thread_set_name(&self->thread, "worker");

	for (;;)
	{
		auto ref = service_next(self->service);
		if (unlikely(ref == NULL))
			break;

		Exception e;
		if (try(&e)) {
			compaction_run(&self->compaction, ref);
		}
		if (catch(&e))
		{ }

		service_complete(self->service, ref);
	}

	return NULL;
}

void
worker_init(Worker* self, Engine* engine)
{
	self->service = &engine->service;
	self->context = NULL;
	compaction_init(&self->compaction, engine);
	thread_init(&self->thread);
}

void
worker_free(Worker* self)
{
	compaction_free(&self->compaction);
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
