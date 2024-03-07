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
	int         state;
	uint64_t    time;
	bool        refresh;
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
	self->state      = ID_NONE;
	self->time       = 0;
	self->refresh    = false;
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
part_set(Part* self, int state)
{
	self->state |= state;
}

static inline void
part_unset(Part* self, int state)
{
	self->state &= ~state;
}

static inline bool
part_has(Part* self, int mask)
{
	return (self->state & mask) > 0;
}

static inline void
part_set_time(Part* self, uint64_t time)
{
	self->time = time;
}

static inline void
part_set_cloud(Part* self, Cloud* cloud)
{
	self->cloud = cloud;
}

static inline Memtable*
part_memtable(Part* self, Memtable** prev)
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
	return heap_used(&self->memtable->heap) > (uint32_t)self->source->refresh_wm;
}
