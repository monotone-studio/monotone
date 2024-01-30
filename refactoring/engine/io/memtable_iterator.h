#pragma once

//
// monotone
//
// time-series storage
//

typedef struct MemtableIterator MemtableIterator;

struct MemtableIterator
{
	Iterator      iterator;
	Row*          current;
	MemtablePage* page;
	int           page_pos;
	Memtable*     memtable;
};

static inline bool
memtable_iterator_open(MemtableIterator* self,
                       Memtable*         memtable,
                       Row*              key)
{
	self->current  = NULL;
	self->page     = NULL;
	self->page_pos = 0;
	self->memtable = memtable;
	if (unlikely(self->memtable->count == 0))
		return false;
	bool match = memtable_seek(memtable, key, &self->page, &self->page_pos);
	if (self->page_pos >= self->page->rows_count)
	{
		self->page     = NULL;
		self->page_pos = 0;
		return false;
	}
	self->current = self->page->rows[self->page_pos];
	return match;
}

static inline bool
memtable_iterator_has(MemtableIterator* self)
{
	return self->current != NULL;
}

static inline Row*
memtable_iterator_at(MemtableIterator* self)
{
	return self->current;
}

static inline void
memtable_iterator_next(MemtableIterator* self)
{
	if (unlikely(self->current == NULL))
		return;

	self->current = NULL;
	if (likely((self->page_pos + 1) < self->page->rows_count))
	{
		self->current = self->page->rows[++self->page_pos];
		return;
	}

	auto prev = self->page;
	self->page_pos = 0;
	self->page     = NULL;

	auto next = rbtree_next(&self->memtable->tree, &prev->node);
	if (unlikely(next == NULL))
		return;

	self->page = container_of(next, MemtablePage, node);
	self->current = self->page->rows[0];
}

static inline void
memtable_iterator_reset(MemtableIterator* self)
{
	self->current  = NULL;
	self->page_pos = 0;
	self->page     = 0;
	self->memtable = NULL;
}

static inline void
memtable_iterator_init(MemtableIterator* self)
{
	self->current  = NULL;
	self->page_pos = 0;
	self->page     = 0;
	self->memtable = NULL;
	auto it = &self->iterator;
	it->has   = (IteratorHas)memtable_iterator_has;
	it->at    = (IteratorAt)memtable_iterator_at;
	it->next  = (IteratorNext)memtable_iterator_next;
	it->close = NULL;
}
