
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

hot void
worker_execute(Worker* self, ServicePart* ref, ServiceReq* req)
{
	(void)self;
	(void)ref;
	(void)req;
#if 0
	auto engine = self->engine;

	// take engine shared lock to prevent any exclusive ddl
	lock_mgr_lock_shared(&engine->lock_mgr);
	guard(lock_guard, lock_mgr_unlock_shared, &engine->lock_mgr);

	// handle drop request
	if (req->type == SERVICE_DROP)	
	{
		return;
	}

	// compact/move request

	// (1) find partition, match storage and rotate memtable
	auto lock = engine_find(engine, false, ref->min);
	assert(lock);
	auto origin = lock->part;

	// match storage, if provided by request
	Storage* storage = NULL;
	if (! str_empty(&req->storage))
	{
		storage = storage_mgr_find(&engine->storage_mgr, &req->storage);
		if (unlikely(! storage))
		{
			lock_mgr_unlock(lock);
			log("compaction: storage <%.*s> no longer available",
			    str_size(&req->storage),
			    str_of(&req->storage));
			return;
		}
	}

	// rotate memtable
	auto memtable = part_memtable_rotate(origin);
	lock_mgr_unlock(lock);

	// get original partition storage, use it if the request storage was 
	// not provided
	auto origin_storage =
		storage_mgr_find(&engine->storage_mgr, &origin->target->name);
	if (storage == NULL)
		storage = origin_storage;

	// (2) create new partition by merging existing partition file
	//     with the memtable
	MergerReq merger_req =
	{
		.origin   = origin,
		.memtable = memtable,
		.target   = storage->target
	};
	merger_reset(&self->merger);
	merger_execute(&self->merger, &merger_req);
	auto part = self->merger.part;

	// (3) replace existing partition with the new one
	lock = engine_find(engine, false, ref->min);
	assert(lock);
	assert(lock->part == origin);

	// update partition tree and storages
	mutex_lock(&engine->lock);

	part_tree_replace(&engine->tree, origin, part);
	storage_remove(origin_storage, origin);
	storage_add(storage, part);

	mutex_unlock(&engine->lock);

	// reuse memtable
	*part->memtable = *origin->memtable;
	memtable_init(origin->memtable,
	              origin->memtable->size_page,
	              origin->memtable->size_split,
	              origin->comparator);

	lock_mgr_unlock(lock);

	// (4) finilize and cleanup
	
	// free memtable memory
	memtable_free(memtable);

	// recovery crash case 1
	//
	// remove and free old partition
	//
	part_delete(origin, true);
	part_free(origin);

	// recovery crash case 2
	//
	// rename new partition
	//
	part_rename(part);
#endif
}

static void*
worker_main(void* arg)
{
	Worker* self = arg;
	runtime_init(self->context);
	thread_set_name(&self->thread, "compaction");

	for (;;)
	{
		ServicePart* part = NULL;
		auto req = service_next(self->service, &part);
		if (unlikely(req == NULL))
			break;

		Exception e;
		if (try(&e))
		{
			worker_execute(self, part, req);
		}
		if (catch(&e))
		{ }

		service_complete(self->service, part);
	}

	return NULL;
}

void
worker_init(Worker* self)
{
	self->engine  = NULL;
	self->service = NULL;
	self->context = NULL;
	merger_init(&self->merger);
	thread_init(&self->thread);
}

void
worker_free(Worker* self)
{
	merger_free(&self->merger);
}

void
worker_start(Worker* self, Service* service, Engine* engine)
{
	self->service = service;
	self->engine  = engine;
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
