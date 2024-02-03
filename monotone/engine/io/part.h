#pragma once

//
// monotone
//
// time-series storage
//

typedef struct Part Part;

struct Part
{
	Id          id;
	bool        refresh;
	bool        in_storage;
	bool        in_cloud;
	Memtable*   memtable;
	Memtable    memtable_a;
	Memtable    memtable_b;
	File        file;
	Index*      index;
	Buf         index_buf;
	Cloud*      cloud;
	Source*     source;
	Comparator* comparator;
	List        link;
};

static inline Part*
part_allocate(Comparator* comparator, Source* source, Id* id)
{
	auto self = (Part*)mn_malloc(sizeof(Part));
	self->id         = *id;
	self->refresh    = false;
	self->in_storage = false;
	self->in_cloud   = false;
	self->index      = NULL;
	self->cloud      = NULL;
	self->source     = source;
	self->comparator = comparator;
	file_init(&self->file);
	self->memtable = &self->memtable_a;
	memtable_init(&self->memtable_a, 512, 508, comparator);
	memtable_init(&self->memtable_b, 512, 508, comparator);
	buf_init(&self->index_buf);
	list_init(&self->link);
	return self;
}

static inline void
part_free(Part* self)
{
	memtable_free(&self->memtable_a);
	memtable_free(&self->memtable_b);
	file_close(&self->file);
	buf_free(&self->index_buf);
	mn_free(self);
}

static inline void
part_set_cloud(Part* self, Cloud* cloud)
{
	self->cloud = cloud;
}

static inline Memtable*
part_memtable_of(Part* self, Memtable** prev)
{
	if (self->memtable == &self->memtable_a)
		*prev = &self->memtable_b;
	else
		*prev = &self->memtable_a;
	return self->memtable;
}

static inline Memtable*
part_memtable_rotate(Part* self)
{
	Memtable* prev = self->memtable;
	Memtable* current;
	if (prev == &self->memtable_a)
		current = &self->memtable_b;
	else
		current = &self->memtable_a;
	self->memtable = current;
	return prev;
}

static inline bool
part_refresh_ready(Part* self)
{
	if (self->refresh)
		return false;
	if (! self->source->refresh_wm)
		return false;
	return self->memtable->size > (uint32_t)self->source->refresh_wm;
}
