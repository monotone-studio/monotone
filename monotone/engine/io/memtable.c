
//
// monotone.
//
// embeddable cloud-native storage for events
// and time-series data.
//
// Copyright (c) 2023-2024 Dmitry Simonenko
// Copyright (c) 2023-2024 Monotone
//
// Distributed under the terms of GNU LGPL v3 or
// Monotone Commercial License.
//

#include <monotone_runtime.h>
#include <monotone_lib.h>
#include <monotone_config.h>
#include <monotone_cloud.h>
#include <monotone_io.h>

void
memtable_init(Memtable*   self,
              int         size_page,
              int         size_split,
              Comparator* comparator)
{
	self->count           = 0;
	self->count_pages     = 0;
	self->size_page       = size_page;
	self->size_split      = size_split;
	self->comparator      = comparator;
	self->lsn_min         = UINT64_MAX;
	self->lsn_max         = 0;
	self->iterators_count = 0;
	rbtree_init(&self->tree);
	list_init(&self->iterators);
	heap_init(&self->heap, global()->memory_mgr);
}

static MemtablePage*
memtable_page_allocate(Memtable* self)
{
	MemtablePage* page;
	page = heap_allocate(&self->heap, sizeof(MemtablePage) + self->size_page * sizeof(Event*));
	page->events_count = 0;
	rbtree_init_node(&page->node);
	return page;
}

always_inline static inline MemtablePage*
memtable_page_of(RbtreeNode* node)
{
	return container_of(node, MemtablePage, node);
}

always_inline static inline int
memtable_compare(Memtable* self, MemtablePage* page, Event* key)
{
	return compare(self->comparator, page->events[0], key);
}

void
memtable_free(Memtable* self)
{
	self->count       = 0;
	self->count_pages = 0;
	self->lsn_min     = UINT64_MAX;
	self->lsn_max     = 0;
	rbtree_init(&self->tree);
	list_init(&self->iterators);
	heap_reset(&self->heap);
}

void
memtable_move(Memtable* self, Memtable* from)
{
	*self = *from;
	assert(! from->iterators_count);
	list_init(&self->iterators);

	from->count       = 0;
	from->count_pages = 0;
	from->lsn_min     = UINT64_MAX;
	from->lsn_max     = 0;
	rbtree_init(&from->tree);
	list_init(&from->iterators);
	heap_init(&from->heap, global()->memory_mgr);
}

hot static inline
rbtree_get(memtable_find, memtable_compare(arg, memtable_page_of(n), key))

hot static inline MemtablePage*
memtable_search_page(Memtable* self, Event* key)
{
	if (self->count_pages == 1)
	{
		auto first = rbtree_min(&self->tree);
		return container_of(first, MemtablePage, node);
	}

	// page[n].min <= key && key < page[n + 1].min
	RbtreeNode* part_ptr = NULL;
	int rc = memtable_find(&self->tree, self, key, &part_ptr);
	assert(part_ptr != NULL);
	if (rc == 1)
	{
		auto prev = rbtree_prev(&self->tree, part_ptr);
		if (prev)
			part_ptr = prev;
	}
	return container_of(part_ptr, MemtablePage, node);
}

hot static inline int
memtable_search(Memtable* self, MemtablePage* page, Event* key, bool* match)
{
	int min = 0;
	int mid = 0;
	int max = page->events_count - 1;

	if (compare(self->comparator, page->events[max], key) < 0)
		return max + 1;

	while (max >= min)
	{
		mid = min + ((max - min) >> 1);
		int rc = compare(self->comparator, page->events[mid], key);
		if (rc < 0) {
			min = mid + 1;
		} else
		if (rc > 0) {
			max = mid - 1;
		} else
		{
			*match = true;
			return mid;
		}
	}
	return min;
}

#if 0
hot static inline int
memtable_search(Memtable* self, MemtablePage* page, void* key, bool* match)
{
	int pos = page->events_count - 1;
	while (pos >= 0)
	{
		int rc = self->compare(self, page->events[pos], key);
		if (rc == 0) {
			*match = true;
			break;
		}
		// range[pos] < key
		if (rc < 0)
			break;
		pos--;
	}
	return pos;
}
#endif

#if 0
hot static inline int
range_search(Ref self, RangeTree* tree, Ref key, bool* match)
{
	int pos = range_of(self, tree)->events_count - 1;
	while (pos >= 0)
	{
		int rc = range_compare(tree, range_key(self, tree, pos), key);
		if (rc == 0) {
			*match = true;
			break;
		}
		// range[pos] < key
		if (rc < 0)
			break;
		pos--;
	}
	return pos;
}
#endif

hot static inline void
memtable_sync(Memtable* self, MemtablePage* page, MemtablePage* split, int pos)
{
	// sync iterators
	list_foreach(&self->iterators)
	{
		auto it = list_at(MemtableIterator, link);
		memtable_iterator_sync(it, page, split, pos);
	}
}

hot static inline Event*
memtable_insert(Memtable*      self,
                MemtablePage*  page,
                MemtablePage** page_split,
                Event*         event)
{
	bool match = false;
	int pos = memtable_search(self, page, event, &match);
	if (match)
	{
		// replace
		auto prev = page->events[pos];
		page->events[pos] = event;

		// update iterators
		if (self->iterators_count > 0)
			memtable_sync(self, page, NULL, pos);

		return prev;
	}
	if (pos < 0)
		pos = 0;

	// split
	MemtablePage* ref = page;
	MemtablePage* l   = page;
	MemtablePage* r   = NULL;
	if (unlikely(page->events_count == self->size_page))
	{
		r = memtable_page_allocate(self);
		l->events_count = self->size_split;
		r->events_count = self->size_page - self->size_split;
		memmove(&r->events[0], &l->events[self->size_split],
		        sizeof(Event*) * r->events_count);
		if (pos >= l->events_count)
		{
			ref = r;
			pos = pos - l->events_count;
		}
	}

	// insert
	int size = (ref->events_count - pos) * sizeof(Event*);
	if (size > 0)
		memmove(&ref->events[pos + 1], &ref->events[pos], size);
	ref->events[pos] = event;
	ref->events_count++;
	atomic_u64_inc(&self->count);

	// update iterators
	if (self->iterators_count > 0)
		memtable_sync(self, l, r, pos);

	*page_split = r;
	return NULL;
}

hot Event*
memtable_set(Memtable* self, Event* event)
{
	// create root page
	if (self->count_pages == 0)
	{
		auto page = memtable_page_allocate(self);
		page->events[0] = event;
		page->events_count++;
		rbtree_set(&self->tree, NULL, 0, &page->node);
		self->count_pages++;
		atomic_u64_inc(&self->count);
		return NULL;
	}

	// search page
	auto page = memtable_search_page(self, event);

	// insert into page
	MemtablePage* page_split = NULL;
	auto prev = memtable_insert(self, page, &page_split, event);
	if (page_split)
	{
		// update split page
		rbtree_set(&self->tree, &page->node, -1, &page_split->node);
		self->count_pages++;
	}
	return prev;
}

void
memtable_unset(Memtable* self, Event* event)
{
	assert(self->count_pages > 0);

	// search page
	auto page = memtable_search_page(self, event);

	// remove existing key from a page
	bool match = false;
	int pos = memtable_search(self, page, event, &match);
	if (! match)
		return;

	// remove event from the page
	page->events[pos] = NULL;
	page->events_count--;
	atomic_u64_dec(&self->count);

	// remove page, if it becomes empty
	if (page->events_count == 0)
	{
		rbtree_remove(&self->tree, &page->node);
		self->count_pages--;
	} else
	{
		int size = (page->events_count - pos) * sizeof(Event*);
		if (size > 0)
			memmove(&page->events[pos], &page->events[pos + 1], size);
	}
}

Event*
memtable_max(Memtable* self)
{
	if (self->count_pages == 0)
		return NULL;
	auto last = rbtree_max(&self->tree);
	auto page = container_of(last, MemtablePage, node);
	return page->events[page->events_count - 1];
}

hot bool
memtable_seek(Memtable* self, Event* key, MemtablePage** page, int* pos)
{
	if (self->count_pages == 0)
		return false;

	if (key == NULL)
	{
		auto first = rbtree_min(&self->tree);
		*page = container_of(first, MemtablePage, node);
		*pos  = 0;
		return false;
	}

	// search page
	*page = memtable_search_page(self, key);

	// search inside page
	bool match = false;
	*pos = memtable_search(self, *page, key, &match);
	return match;
}

void
memtable_follow(Memtable* self, uint64_t lsn)
{
	if (lsn < atomic_u64_of(&self->lsn_min))
		atomic_u64_set(&self->lsn_min, lsn);

	if (lsn > atomic_u64_of(&self->lsn_max))
		atomic_u64_set(&self->lsn_max, lsn);
}
