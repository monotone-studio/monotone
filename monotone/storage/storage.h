#pragma once

//
// monotone
//
// time-series storage
//

typedef struct Storage Storage;

struct Storage
{
	List    list;
	int     list_count;
	int     refs;
	Target* target;
	List    link;
};

static inline void
storage_free(Storage* self)
{
	if (self->target)
		target_free(self->target);
	mn_free(self);
}

static inline Storage*
storage_allocate(Target* target)
{
	Storage* self = mn_malloc(sizeof(Storage));
	self->target     = NULL;
	self->refs       = 0;
	self->list_count = 0;
	list_init(&self->list);
	guard(self_guard, storage_free, self);
	self->target     = target_copy(target);
	return unguard(&self_guard);
}

static inline void
storage_ref(Storage* self)
{
	self->refs++;
}

static inline void
storage_unref(Storage* self)
{
	self->refs--;
	assert(self->refs >= 0);
}

static inline void
storage_add(Storage* self, Part* part)
{
	list_append(&self->list, &part->link_storage);
	self->list_count++;
}

static inline void
storage_remove(Storage* self, Part* part)
{
	assert(part->target == self->target);
	list_unlink(&part->link_storage);
	self->list_count--;
}

hot static inline Part*
storage_find(Storage* self, uint64_t min)
{
	list_foreach(&self->list)
	{
		auto part = list_at(Part, link_storage);
		if (part->min == min)
			return part;
	}
	return NULL;
}

hot static inline Part*
storage_min(Storage* self)
{
	Part* min = NULL;
	list_foreach(&self->list)
	{
		auto part = list_at(Part, link_storage);
		if (min == NULL)
			min = part;
		else
		if (part->min < min->min)
			min = part;
	}
	return min;
}
