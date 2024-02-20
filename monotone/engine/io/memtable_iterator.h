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
	Event*        current;
	MemtablePage* page;
	int           page_pos;
	Memtable*     memtable;
	List          link;
};

static inline bool
memtable_iterator_open(MemtableIterator* self,
                       Memtable*         memtable,
                       Event*            key,
                       bool              attach)
{
	self->current  = NULL;
	self->page     = NULL;
	self->page_pos = 0;
	self->memtable = memtable;
	if (unlikely(self->memtable->count == 0))
		return false;
	bool match = memtable_seek(memtable, key, &self->page, &self->page_pos);
	if (self->page_pos >= self->page->events_count)
	{
		self->page     = NULL;
		self->page_pos = 0;
		return false;
	}
	self->current = self->page->events[self->page_pos];

	// attach iterator to the memtable for updates
	if (attach)
	{
		list_append(&memtable->iterators, &self->link);
		memtable->iterators_count++;
	}
	return match;
}

static inline bool
memtable_iterator_has(MemtableIterator* self)
{
	return self->current != NULL;
}

static inline Event*
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
	if (likely((self->page_pos + 1) < self->page->events_count))
	{
		self->current = self->page->events[++self->page_pos];
		return;
	}

	auto prev = self->page;
	self->page_pos = 0;
	self->page     = NULL;

	auto next = rbtree_next(&self->memtable->tree, &prev->node);
	if (unlikely(next == NULL))
		return;

	self->page = container_of(next, MemtablePage, node);
	self->current = self->page->events[0];
}

static inline void
memtable_iterator_reset(MemtableIterator* self)
{
	self->current  = NULL;
	self->page_pos = 0;
	self->page     = 0;
	self->memtable = NULL;
	list_init(&self->link);
}

static inline void
memtable_iterator_close(MemtableIterator* self)
{
	// detach from memtable
	if (! list_empty(&self->link))
	{
		list_unlink(&self->link);
		self->memtable->iterators_count--;
		assert(self->memtable->iterators_count >= 0);
	}
	memtable_iterator_reset(self);
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
	it->close = (IteratorClose)memtable_iterator_close;
	list_init(&self->link);
}

static inline void
memtable_iterator_sync(MemtableIterator* self,
                       MemtablePage*     page,
                       MemtablePage*     page_split,
                       int               pos)
{
	if (self->page != page || !self->current)
		return;

	// replace
	if (self->page_pos == pos)
	{
		// update current page event pointer on replace
		self->current = self->page->events[self->page_pos];
		return;
	}

	// insert
	if (page_split)
	{
		if (self->page_pos >= page->events_count)
		{
			self->page = page_split;
			self->page_pos = self->page_pos - page->events_count;
		}
	}

	// move position, if key was inserted before it
	if (pos <= self->page_pos)
		self->page_pos++;

	self->current = self->page->events[self->page_pos];
}
