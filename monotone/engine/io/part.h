#pragma once

//
// monotone
//
// time-series storage
//

typedef struct PartId PartId;
typedef struct Part   Part;

struct PartId
{
	uint64_t id;
	uint64_t id_parent;
	uint64_t min;
	uint64_t max;
};

struct Part
{
	bool        refresh;
	PartId      id;
	Memtable*   memtable;
	Memtable    memtable_a;
	Memtable    memtable_b;
	File        file;
	Index*      index;
	Buf         index_buf;
	Source*     source;
	Comparator* comparator;
	List        link;
};

Part*
part_allocate(Comparator*, Source*, PartId*);
void part_free(Part*);
void part_open(Part*);
void part_create(Part*);
void part_delete(Part*, bool);
void part_rename(Part*);

Part*
part_download(Cloud*, Comparator*, Source*, PartId*);

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
