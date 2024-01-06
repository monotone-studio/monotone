
//
// monotone
//
// time-series storage
//

#include <monotone_runtime.h>
#include <monotone_lib.h>
#include <monotone_io.h>
#include <monotone_engine.h>

hot void
compaction_execute(Compaction* self, ServiceReq* req, Tier* tier)
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
	Storage* storage;
	if (tier) {
		storage = tier->storage;
	} else {
		tier = tier_of(&engine->tier_mgr, origin->storage->order);
		storage = origin->storage;
	}

	// drop
	if (storage->capacity == 0)
	{
		auto lock = engine_find(engine, false, req->min);
		assert(lock);

		auto origin_tier = tier_of(&engine->tier_mgr, origin->storage->order);

		mutex_lock(&engine->lock);
		part_tree_remove(&engine->tree, origin);
		list_unlink(&origin->link);
		engine->list_count--;

		tier_remove(origin_tier, origin);
		mutex_unlock(&engine->lock);

		lock_mgr_unlock(lock);

		// remove and free old partition
		if (origin->storage->memory)
			part_unload(origin);
		part_delete(origin, true);
		part_free(origin);
		return;
	}

	MergerReq merger_req =
	{
		.origin   = origin,
		.memtable = memtable,
		.storage  = storage
	};
	merger_reset(&self->merger);
	auto part = merger_execute(&self->merger, &merger_req);
	part->service = true;

	// stage 3. replace existing partition with the new one
	lock = engine_find(engine, false, req->min);
	assert(lock);
	assert(lock->arg == origin);

	mutex_lock(&engine->lock);

	// update partition tree and list
	part_tree_replace(&engine->tree, origin, part);
	list_unlink(&origin->link);
	list_append(&engine->list, &part->link);

	// update tiers
	auto origin_tier = tier_of(&engine->tier_mgr, origin->storage->order);
	tier_remove(origin_tier, origin);
	tier_add(tier, part);

	mutex_unlock(&engine->lock);

	// reuse memtable
	*part->memtable = *origin->memtable;
	memtable_init(origin->memtable,
	              origin->memtable->size_page,
	              origin->memtable->size_split,
	              origin->comparator);

	lock_mgr_unlock(lock);

	// stage 4. finilize and cleanup
	
	// free memtable memory
	memtable_free(memtable);

	// create partition file and sync
	part_create(part, storage->sync);

	if (! storage->memory)
		part_unload(part);

	// remove and free old partition
	part_delete(origin, true);
	part_free(origin);

	// rename new partition
	part_rename(part);

	// make new partition available for the service
	mutex_lock(&engine->lock);
	part->service = false;
	mutex_unlock(&engine->lock);
}

hot static inline bool
compaction_rebalance_next(Compaction* self, ServiceReq* req, Tier** req_tier)
{
	auto engine = self->engine;
	mutex_lock(&engine->lock);

	bool match = false;
	for (int order = 0; order < engine->tier_mgr.tiers_count; order++)
	{
		auto tier = tier_of(&engine->tier_mgr, order);
		auto storage = tier->storage;
		if (storage->capacity == INT_MAX ||
		    storage->capacity == 0)
			break;

		assert(engine->tier_mgr.tiers_count > order + 1);
		if (tier->list_count >= storage->capacity)
		{
			// find min partition
			auto part = tier_min(tier);
			if (! part->service)
			{
				req->min  = part->min;
				req->max  = part->max;
				*req_tier = tier_of(&engine->tier_mgr, order + 1);
				part->service = true;
				match = true;
				break;
			}
		}
	}

	mutex_unlock(&engine->lock);
	return match;
}

hot void
compaction_rebalance(Compaction* self)
{
	if (self->engine->tier_mgr.tiers_count == 1)
		return;

	for (;;)
	{
		ServiceReq req;
		service_req_init(&req);
		Tier* tier = NULL;
		if (! compaction_rebalance_next(self, &req, &tier))
			break;
		compaction_execute(self, &req, tier);
	}
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
		if (try(&e))
		{
			// rebalance tiers first
			compaction_rebalance(self);

			// compaction
			if (! service_req_rebalance(req))
				compaction_execute(self, req, NULL);
		}
		if (catch(&e))
		{ }

		service_req_free(req);
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
	self->global  = mn_runtime.global;
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
