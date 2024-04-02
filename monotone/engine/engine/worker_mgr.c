
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
worker_mgr_init(WorkerMgr* self)
{
	self->workers       = NULL;
	self->workers_count = 0;
}

void
worker_mgr_start(WorkerMgr* self, Engine* engine)
{
	int workers = config_workers();
	int workers_upload = config_workers_upload();

	self->workers_count = workers + workers_upload;
	self->workers = mn_malloc(sizeof(Worker) * self->workers_count);

	int i = 0;
	if (workers_upload == 0)
	{
		for (; i < workers; i++)
			worker_init(&self->workers[i], engine, SERVICE_ANY);
	} else
	{
		for (; i < workers; i++)
			worker_init(&self->workers[i], engine, SERVICE_WITHOUT_UPLOAD);
		for (; i < self->workers_count; i++)
			worker_init(&self->workers[i], engine, SERVICE_UPLOAD);
	}
	for (i = 0; i < self->workers_count; i++)
		worker_start(&self->workers[i]);
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
