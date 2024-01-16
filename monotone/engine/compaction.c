
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
compaction_init(Compaction* self, Engine* engine)
{
	self->ref            = NULL;
	self->req            = NULL;
	self->origin         = NULL;
	self->part           = NULL;
	self->memtable       = NULL;
	self->storage_origin = NULL;
	self->storage        = NULL;
	self->engine         = engine;
	merger_init(&self->merger);
}

void
compaction_free(Compaction* self)
{
	merger_free(&self->merger);
}

void
compaction_reset(Compaction* self)
{
	self->ref            = NULL;
	self->req            = NULL;
	self->origin         = NULL;
	self->part           = NULL;
	self->memtable       = NULL;
	self->storage_origin = NULL;
	self->storage        = NULL;
	merger_reset(&self->merger);
}

static void
compaction_drop(Compaction* self)
{
	auto engine = self->engine;

	// find the original partition
	auto lock = engine_find(engine, false, self->ref->min);
	if (unlikely(! lock))
		error("compaction: partition <%" PRIu64 "> is no longer available",
		      self->ref->min);

	// set origin
	self->origin = lock->part;

	// get the original partition storage
	self->storage_origin =
		storage_mgr_find(&engine->storage_mgr, &self->origin->target->name);

	// remove partition from the partition tree and storage
	mutex_lock(&engine->lock);
	part_tree_remove(&engine->tree, self->origin);
	storage_remove(self->storage_origin, self->origin);
	mutex_unlock(&engine->lock);

	lock_mgr_unlock(lock);

	// delete partition file and free
	part_delete(self->origin, true);
	part_free(self->origin);
	self->origin = NULL;
}

static void
compaction_rotate(Compaction* self)
{
	auto engine = self->engine;
	auto req = self->req;

	// match storage, if provided by request
	if (! str_empty(&self->req->storage))
	{
		self->storage = storage_mgr_find(&self->engine->storage_mgr, &req->storage);
		if (unlikely(! self->storage))
		{
			error("compaction: storage <%.*s> is no longer available",
			      str_size(&req->storage),
			      str_of(&req->storage));
		}
	}

	// find the original partition
	auto lock = engine_find(engine, false, self->ref->min);
	if (unlikely(! lock))
		error("compaction: partition <%" PRIu64 "> is no longer available",
		      self->ref->min);

	// set origin
	self->origin = lock->part;

	// rotate memtable
	self->memtable = part_memtable_rotate(self->origin);
	lock_mgr_unlock(lock);

	// get the original partition storage
	self->storage_origin =
		storage_mgr_find(&engine->storage_mgr, &self->origin->target->name);

	// set storage, if not provided by request
	if (self->storage == NULL)
		self->storage = self->storage_origin;
}

static void
compaction_merge(Compaction* self)
{
	MergerReq merger_req =
	{
		.origin   = self->origin,
		.memtable = self->memtable,
		.target   = self->storage->target
	};
	merger_execute(&self->merger, &merger_req);
	self->part = self->merger.part;
	self->merger.part = NULL;
}

static void
compaction_apply(Compaction* self)
{
	auto engine = self->engine;
	auto origin = self->origin;
	auto part = self->part;

	auto lock = engine_find(engine, false, self->ref->min);
	assert(lock);
	assert(lock->part == origin);

	mutex_lock(&engine->lock);

	// update partition tree
	part_tree_replace(&engine->tree, origin, part);

	// remove old partition from its storage
	storage_remove(self->storage_origin, origin);

	// add new partition to the storage
	storage_add(self->storage, part);

	mutex_unlock(&engine->lock);

	// reuse memtable
	*self->part->memtable = *origin->memtable;
	memtable_init(origin->memtable,
	              origin->memtable->size_page,
	              origin->memtable->size_split,
	              origin->comparator);

	lock_mgr_unlock(lock);
}

static void
compaction_finish(Compaction* self)
{
	auto origin = self->origin;
	auto part = self->part;

	// free memtable memory
	memtable_free(self->memtable);
	self->memtable = NULL;

	// recovery crash case 1

	// remove and free old partition
	part_delete(origin, true);
	part_free(origin);
	self->origin = NULL;

	// recovery crash case 2

	// rename new partition
	part_rename(part);
}

void
compaction_run(Compaction* self, ServicePart* ref)
{
	self->ref = ref;
	self->req = service_part_req(ref);

	// take engine shared lock to prevent any exclusive ddl
	// during the process
	lock_mgr_lock_shared(&self->engine->lock_mgr);
	guard(lock_guard, lock_mgr_unlock_shared, &self->engine->lock_mgr);

	// handle partition drop request
	if (self->req->type == SERVICE_DROP)
	{
		compaction_drop(self);
		return;
	}

	// handle partition merge/move request

	// step 1. find partition, match storage and rotate memtable
	compaction_rotate(self);

	// step 2. create new partition by merging existing partition
	// file with the memtable
	compaction_merge(self);

	// step 3. replace existing partition with the new one
	compaction_apply(self);

	// step 4. finilize and cleanup
	compaction_finish(self);
}
