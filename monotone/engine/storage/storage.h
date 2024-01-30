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
	Source* source;
	List    link;
};

static inline void
storage_free(Storage* self)
{
	if (self->source)
		source_free(self->source);
	mn_free(self);
}

static inline Storage*
storage_allocate(Source* source)
{
	Storage* self = mn_malloc(sizeof(Storage));
	self->source     = NULL;
	self->refs       = 0;
	self->list_count = 0;
	list_init(&self->list);
	guard(self_guard, storage_free, self);
	self->source     = source_copy(source);
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
	list_append(&self->list, &part->link);
	self->list_count++;
}

static inline void
storage_remove(Storage* self, Part* part)
{
	assert(part->source == self->source);
	list_unlink(&part->link);
	self->list_count--;
}

hot static inline Part*
storage_find(Storage* self, uint64_t min)
{
	list_foreach(&self->list)
	{
		auto part = list_at(Part, link);
		if (part->id.min == min)
			return part;
	}
	return NULL;
}

hot static inline Part*
storage_oldest(Storage* self)
{
	Part* min = NULL;
	list_foreach(&self->list)
	{
		auto part = list_at(Part, link);
		if (min == NULL)
			min = part;
		else
		if (part->id.id < min->id.id)
			min = part;
	}
	return min;
}

static inline void
storage_show_partitions(Storage* self, Buf* buf)
{
	buf_printf(buf, "%.*s\n", str_size(&self->source->name),
	           str_of(&self->source->name));
	list_foreach(&self->list)
	{
		auto part = list_at(Part, link);
		uint64_t size_cached = part->memtable_a.size + part->memtable_b.size;
		buf_printf(buf, "  [%20" PRIu64",%20" PRIu64 "] size: %" PRIu64 ", size_cached: %" PRIu64 "\n",
		           part->id.min, part->id.max, part->file.size, size_cached);
	}
}
