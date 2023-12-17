
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
engine_init(Engine* self, Comparator* comparator, Service* service)
{
	self->list_count = 0;
	self->service    = service;
	self->comparator = comparator;
	mutex_init(&self->lock);
	list_init(&self->list);
	part_tree_init(&self->tree, comparator);
	lock_mgr_init(&self->lock_mgr);
}

void
engine_free(Engine* self)
{
	assert(! self->list_count);
	lock_mgr_free(&self->lock_mgr);
	mutex_free(&self->lock);
}

void
engine_open(Engine* self)
{
	engine_recover(self);
}

void
engine_close(Engine* self)
{
	list_foreach_safe(&self->list)
	{
		auto part = list_at(Part, link);
		part_free(part);
	}
	self->list_count = 0;
	list_init(&self->list);
}

void
engine_flush(Engine* self)
{
	mutex_lock(&self->lock);
	guard(unlock, mutex_unlock, &self->lock);

	list_foreach(&self->list)
	{
		auto part = list_at(Part, link);
		if (part->service)
			continue;
		if (part->memtable->size > 0)
		{
			part->service = true;
			service_add(self->service, part->min, part->max);
		}
	}
}

hot Lock*
engine_find(Engine* self, bool create, uint64_t time)
{
	// lock partition by min
	auto lock = lock_mgr_get(&self->lock_mgr, time);

	mutex_lock(&self->lock);
	guard(unlock, mutex_unlock, &self->lock);

	// find partition by min
	Part* part = NULL;
	if (likely(self->list_count > 0))
	{
		part = part_tree_match(&self->tree, time);
		if (part)
		{
			lock->arg = part;
			return lock;
		}
	}
	if (! create)
		return NULL;

	// create new partition
	part = part_allocate(self->comparator,
	                     time,
	                     time + config()->interval);
	list_append(&self->list, &part->link);
	self->list_count++;
	part_tree_add(&self->tree, part);
	lock->arg = part;
	return lock;
}
