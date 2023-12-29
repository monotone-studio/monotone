#pragma once

//
// monotone
//
// time-series storage
//

typedef struct MemtableIterator MemtableIterator;

struct MemtableIterator
{
	Iterator     iterator;
	MemtableRow* current;
	RbtreeNode*  pos;
	Memtable*    memtable;
};

static inline bool
memtable_iterator_open(MemtableIterator* self,
                       Memtable*         memtable,
                       Row*              row)
{
	self->current  = NULL;
	self->pos      = NULL;
	self->memtable = memtable;
	if (unlikely(self->memtable->count == 0))
		return false;

	if (row == NULL)
	{
		self->pos = rbtree_min(&memtable->index);
		self->current = memtable_row_of(self->pos);
		return false;
	}

	int rc;
	rc = memtable_match(&memtable->index, memtable->comparator, row, &self->pos);
	if (self->pos == NULL)
		return false;
	if (rc == -1)
		self->pos = rbtree_next(&self->memtable->index, self->pos);
	if (self->pos)
		self->current = memtable_row_of(self->pos);

	// match
	return rc == 0 && self->pos;
}

static inline bool
memtable_iterator_has(MemtableIterator* self)
{
	return self->current != NULL;
}

static inline Row*
memtable_iterator_at(MemtableIterator* self)
{
	if (unlikely(self->current == NULL))
		return NULL;
	return &self->current->row;
}

static inline void
memtable_iterator_next(MemtableIterator* self)
{
	if (unlikely(self->current == NULL))
		return;
	self->current = NULL;
	self->pos = rbtree_next(&self->memtable->index, self->pos);
	if (self->pos)
		self->current = memtable_row_of(self->pos);
}

static inline void
memtable_iterator_reset(MemtableIterator* self)
{
	self->current  = NULL;
	self->pos      = 0;
	self->memtable = NULL;
}

static inline void
memtable_iterator_init(MemtableIterator* self)
{
	self->current  = NULL;
	self->pos      = 0;
	self->memtable = NULL;
	auto it = &self->iterator;
	it->has   = (IteratorHas)memtable_iterator_has;
	it->at    = (IteratorAt)memtable_iterator_at;
	it->next  = (IteratorNext)memtable_iterator_next;
	it->close = NULL;
}
