
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
compaction_mgr_init(CompactionMgr* self)
{
	self->workers       = NULL;
	self->workers_count = 0;
}

void
compaction_mgr_start(CompactionMgr* self, Service* service, Engine* engine)
{
	self->workers_count = config_workers();
	self->workers = mn_malloc(sizeof(Compaction) * self->workers_count);
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
	mn_free(self->workers);
	self->workers = NULL;
	self->workers_count = 0;
}
