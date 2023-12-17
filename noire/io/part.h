#pragma once

//
// noire
//
// time-series storage
//

typedef struct Part Part;

struct Part
{
	bool        service;
	uint64_t    min;
	uint64_t    max;
	Memtable*   memtable;
	Memtable    memtable_a;
	Memtable    memtable_b;
	Blob        mmap;
	File        file;
	Index*      index;
	Buf         index_buf;
	Part*       l;
	Part*       r;
	Comparator* comparator;
	RbtreeNode  node;
	List        link_service;
	List        link;
};

static inline Part*
part_allocate(Comparator* comparator,
              uint64_t    min,
              uint64_t    max)
{
	auto self = (Part*)nr_malloc(sizeof(Part));
	self->min        = min;
	self->max        = max;
	self->service    = false;
	self->index      = NULL;
	self->l          = NULL;
	self->r          = NULL;
	self->comparator = comparator;
	blob_init(&self->mmap, 1 * 1024 * 1024);
	file_init(&self->file);
	self->memtable = &self->memtable_a;
	memtable_init(&self->memtable_a, comparator);
	memtable_init(&self->memtable_b, comparator);
	buf_init(&self->index_buf);
	rbtree_init_node(&self->node);
	list_init(&self->link_service);
	list_init(&self->link);
	return self;
}

static inline void
part_free_data(Part* self)
{
	memtable_free(&self->memtable_a);
	memtable_init(&self->memtable_a, NULL);
	memtable_free(&self->memtable_b);
	memtable_init(&self->memtable_b, NULL);
	blob_free(&self->mmap);
	file_close(&self->file);
	buf_free(&self->index_buf);
	buf_init(&self->index_buf);
}

static inline void
part_free(Part* self)
{
	part_free_data(self);
	nr_free(self);
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

void part_open(Part*, bool);
void part_create(Part*);
void part_unload(Part*);
void part_sync(Part*);
void part_delete(Part*, bool);
void part_complete(Part*);
