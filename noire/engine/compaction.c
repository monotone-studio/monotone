
//
// noire
//
// time-series storage
//

#include <noire_runtime.h>
#include <noire_lib.h>
#include <noire_io.h>
#include <noire_engine.h>

hot static inline void
compaction_execute(Compaction* self, ServiceReq* req)
{
	auto engine = self->engine;

	// stage 1. find partition and rotate memtable
	auto lock = engine_find(engine, false, req->min);
	assert(lock);
	Part* origin = lock->arg;
	auto memtable = part_memtable_rotate(origin);
	lock_mgr_unlock(lock);

	// stage 2. create new partition by merging existing
	//          partition file with the memtable
	MergerReq merger_req =
	{
		.origin      = origin,
		.memtable    = memtable,
		.directory   = config_directory(),
		.compression = config()->compression,
		.crc         = config()->crc,
		.region_size = config()->region_size
	};
	merger_reset(&self->merger);
	auto part = merger_execute(&self->merger, &merger_req);
	part->service = true;

	// stage 3. replace existing partition with the new one,
	//          reuse current memtable
	lock = engine_find(engine, false, req->min);
	assert(lock);
	assert(lock->arg == origin);

	mutex_lock(&engine->lock);

	part_tree_replace(&engine->tree, origin, part);
	list_unlink(&origin->link);
	list_append(&engine->list, &part->link);

	mutex_unlock(&engine->lock);

	*part->memtable = *origin->memtable;
	memtable_init(origin->memtable, origin->comparator);

	lock_mgr_unlock(lock);

	// stage 4. finilize and cleanup
	
	// free memtable memory
	memtable_free(memtable);
	memtable_init(memtable, NULL);

	// sync new partition
	/*part_sync(part);*/

	// remove and free old partition
	/*part_delete(origin, true);*/
	part_free(origin);

	// rename new partition
	/*part_complete(part);*/

	// make new partition available for service
	mutex_lock(&engine->lock);
	part->service = false;
	mutex_unlock(&engine->lock);
}

static void*
compaction_main(void* arg)
{
	Compaction* self = arg;
	runtime_init(self->global);

	thread_set_name(&self->thread, "compaction");
	for (;;)
	{
		auto req = service_next(self->service);
		if (unlikely(req == NULL))
			break;

		Exception e;
		if (try(&e)) {
			compaction_execute(self, req);
		}
		if (catch(&e))
		{ }
	}

	return NULL;
}

void
compaction_init(Compaction* self)
{
	self->engine  = NULL;
	self->service = NULL;
	merger_init(&self->merger);
	thread_init(&self->thread);
}

void
compaction_free(Compaction* self)
{
	merger_free(&self->merger);
}

void
compaction_start(Compaction* self, Service* service, Engine* engine)
{
	self->service = service;
	self->engine  = engine;
	self->global  = nr_runtime.global;
	int rc;
	rc = thread_create(&self->thread, compaction_main, self);
	if (unlikely(rc == -1))
		error_system();
}

void
compaction_stop(Compaction* self)
{
	thread_join(&self->thread);
}
