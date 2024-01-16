
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

void
worker_mgr_init(WorkerMgr* self)
{
	self->workers       = NULL;
	self->workers_count = 0;
}

void
worker_mgr_start(WorkerMgr* self, Service* service, Engine* engine)
{
	self->workers_count = config_workers();
	self->workers = mn_malloc(sizeof(Worker) * self->workers_count);
	for (int i = 0; i < self->workers_count; i++)
		worker_init(&self->workers[i]);
	for (int i = 0; i < self->workers_count; i++)
		worker_start(&self->workers[i], service, engine);
}

void
worker_mgr_stop(WorkerMgr* self)
{
	for (int i = 0; i < self->workers_count; i++)
	{
		worker_stop(&self->workers[i]);
		worker_free(&self->workers[i]);
	}
	mn_free(self->workers);
	self->workers = NULL;
	self->workers_count = 0;
}
