#pragma once

//
// monotone
//
// time-series storage
//

typedef struct Part Part;

struct Part
{
	bool        service;
	int         state;
	uint64_t    min;
	uint64_t    max;
	Memtable*   memtable;
	Memtable    memtable_a;
	Memtable    memtable_b;
	File        file;
	Index*      index;
	Buf         index_buf;
	Target*     target;
	Comparator* comparator;
	RbtreeNode  node;
	List        link_tier;
	List        link;
};

Part*
part_allocate(Comparator*, Target*, uint64_t, uint64_t);
void part_free(Part*);
void part_open(Part*, bool);
void part_create(Part*);
void part_delete(Part*, bool);
void part_rename(Part*);

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