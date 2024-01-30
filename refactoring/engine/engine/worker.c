
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

static void*
worker_main(void* arg)
{
	Worker* self = arg;
	runtime_init(self->context);
	thread_set_name(&self->thread, "worker");

	auto service = self->engine->service;
	for (;;)
	{
		ServiceReq* req = NULL;
		auto type = service_next(service, &req);
		if (type == SERVICE_SHUTDOWN)
			break;

		Exception e;
		if (try(&e))
		{
			// always doing rebalance first
			engine_rebalance(self->engine, &self->refresh);

			if (type == SERVICE_REFRESH)
				engine_refresh(self->engine, &self->refresh, req->min, true);
		}
		if (catch(&e))
		{ }

		if (req)
			service_req_free(req);
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
