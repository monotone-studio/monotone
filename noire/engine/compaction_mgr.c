
//
// noire
//
// time-series storage
//

#include <noire_runtime.h>
#include <noire_lib.h>
#include <noire_io.h>
#include <noire_engine.h>

void
compaction_mgr_init(CompactionMgr* self)
{
	self->workers       = NULL;
	self->workers_count = 0;
}

void
compaction_mgr_start(CompactionMgr* self, Service* service, Engine* engine)
{
	self->workers_count = config()->workers;
	self->workers = nr_malloc(sizeof(Compaction) * self->workers_count);
	for (int i = 0; i < self->workers_count; i++)
		compaction_init(&self->workers[i]);
	for (int i = 0; i < self->workers_count; i++)
		compaction_start(&self->workers[i], service, engine);
}

void
compaction_mgr_stop(CompactionMgr* self)
{
	for (int i = 0; i < self->workers_count; i++)
	{
		compaction_stop(&self->workers[i]);
		compaction_free(&self->workers[i]);
	}
	nr_free(self->workers);
	self->workers = NULL;
	self->workers_count = 0;
}
