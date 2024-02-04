
//
// monotone
//
// time-series storage
//

#include <monotone_runtime.h>
#include <monotone_lib.h>
#include <monotone_config.h>
#include <monotone_io.h>
#include <monotone_storage.h>
#include <monotone_engine.h>

void
refresh_init(Refresh* self, Engine* engine)
{
	self->ref            = NULL;
	self->origin         = NULL;
	self->part           = NULL;
	self->memtable       = NULL;
	self->storage_origin = NULL;
	self->storage        = NULL;
	self->engine         = engine;
	merger_init(&self->merger);
}

void
refresh_free(Refresh* self)
{
	merger_free(&self->merger);
}

void
refresh_reset(Refresh* self)
{
	self->ref            = NULL;
	self->origin         = NULL;
	self->part           = NULL;
	self->memtable       = NULL;
	self->storage_origin = NULL;
	self->storage        = NULL;
	merger_reset(&self->merger);
}

static bool
refresh_begin(Refresh* self, uint64_t min, Str* storage, bool if_exists)
{
	auto engine = self->engine;

	// find the original partition
	auto ref = engine_lock(engine, min, LOCK_SERVICE, false, false);
	if (unlikely(! ref))
	{
		if (! if_exists)
			error("refresh: partition <%" PRIu64 "> not found", min);
		return false;
	}

	// ensure partition is offloaded
	if (part_has(ref->part, PART_FILE_CLOUD))
	{
		engine_unlock(engine, ref, LOCK_SERVICE);
		error("refresh: partition <%" PRIu64 "> must be dropped from cloud first", min);
	}

	// match storage, if provided by request
	if (storage)
	{
		self->storage = storage_mgr_find(&engine->storage_mgr, storage);
		if (unlikely(! self->storage))
		{
			engine_unlock(engine, ref, LOCK_SERVICE);
			error("refresh: storage <%.*s> not found", str_size(storage),
			      str_of(storage));
		}
	}

	// set origin
	self->ref    = ref;
	self->origin = ref->part;

	// get the original partition storage
	self->storage_origin =
		storage_mgr_find(&engine->storage_mgr, &self->origin->source->name);

	// if partition has the same storage already, do nothing
	if (self->storage == self->storage_origin)
	{
		engine_unlock(engine, ref, LOCK_SERVICE);
		return false;
	}

	// set storage, if not provided by request
	if (self->storage == NULL)
		self->storage = self->storage_origin;

	// rotate memtable
	mutex_lock(&engine->lock);
	ref_lock(ref, LOCK_ACCESS);

	self->memtable = part_memtable_rotate(self->origin);

	ref_unlock(ref, LOCK_ACCESS);
	mutex_unlock(&engine->lock);

	// keeping service lock till the end of refresh
	return true;
}

static void
refresh_merge(Refresh* self)
{
	MergerReq merger_req =
	{
		.origin   = self->origin,
		.memtable = self->memtable,
		.source   = self->storage->source
	};
	merger_execute(&self->merger, &merger_req);
	self->part = self->merger.part;
	self->merger.part = NULL;
}

static void
refresh_apply(Refresh* self)
{
	auto engine = self->engine;
	auto origin = self->origin;
	auto part   = self->part;

	// update catalog
	mutex_lock(&engine->lock);
	ref_lock(self->ref, LOCK_ACCESS);

	// update partition reference
	self->ref->part = self->part;

	// remove old partition from its storage
	storage_remove(self->storage_origin, origin);

	// add new partition to the storage
	storage_add(self->storage, part);

	// reuse memtable
	*part->memtable = *origin->memtable;
	memtable_init(origin->memtable,
	              origin->memtable->size_page,
	              origin->memtable->size_split,
	              origin->comparator);

	ref_unlock(self->ref, LOCK_ACCESS);
	mutex_unlock(&engine->lock);
}

static void
refresh_end(Refresh* self)
{
	auto origin = self->origin;
	auto part   = self->part;

	// free memtable memory
	memtable_free(self->memtable);
	self->memtable = NULL;

	// recovery crash case 1

	// remove and free old partition
	part_file_delete(origin, true);
	part_free(origin);
	self->origin = NULL;

	// recovery crash case 2

	// rename new partition
	part_file_complete(part);
	self->part = NULL;
}

void
refresh_run(Refresh* self, uint64_t min, Str* storage, bool if_exists)
{
	// step 1. find partition and take the service lock
	//         match storage
	//         rotate memtable
	//
	//         on refresh
	//           do nothing if memtable is empty
	//
	//         on move
	//           do nothing if partition storage not changed
	//
	if (! refresh_begin(self, min, storage, if_exists))
		return;

	Exception e;
	if (try(&e))
	{
		// step 2. create new partition by merging existing partition
		// file with the memtable
		refresh_merge(self);

		// step 3. replace existing partition with the new one
		refresh_apply(self);

		// step 4. finilize and cleanup
		refresh_end(self);
	}

	// complete
	engine_unlock(self->engine, self->ref, LOCK_SERVICE);
	self->ref = NULL;

	if (catch(&e))
		rethrow();
}
