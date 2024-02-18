
//
// monotone
//
// time-series storage
//

#include <monotone_runtime.h>
#include <monotone_lib.h>
#include <monotone_config.h>
#include <monotone_io.h>

void
memtable_init(Memtable*   self,
              int         size_page,
              int         size_split,
              Comparator* comparator)
{
	self->count           = 0;
	self->count_pages     = 0;
	self->size            = 0;
	self->size_page       = size_page;
	self->size_split      = size_split;
	self->comparator      = comparator;
	self->lsn_min         = UINT64_MAX;
	self->lsn_max         = 0;
	self->iterators_count = 0;
	rbtree_init(&self->tree);
	list_init(&self->iterators);
	heap_init(&self->heap, 2 * 1024 * 1024);
}

static MemtablePage*
memtable_page_allocate(Memtable* self)
{
	MemtablePage* page;
	page = heap_allocate(&self->heap, sizeof(MemtablePage) + self->size_page * sizeof(Row*));
	page->rows_count = 0;
	rbtree_init_node(&page->node);
	return page;
}

always_inline static inline MemtablePage*
memtable_page_of(RbtreeNode* node)
{
	return container_of(node, MemtablePage, node);
}

always_inline static inline int
memtable_compare(Memtable* self, MemtablePage* page, Row* key)
{
	return compare(self->comparator, page->rows[0], key);
}

void
memtable_free(Memtable* self)
{
	self->count       = 0;
	self->count_pages = 0;
	self->size        = 0;
	self->lsn_min     = UINT64_MAX;
	self->lsn_max     = 0;
	rbtree_init(&self->tree);
	list_init(&self->iterators);
	heap_free(&self->heap);
}

void
memtable_move(Memtable* self, Memtable* from)
{
	*self = *from;
	assert(! from->iterators_count);
	list_init(&self->iterators);

	from->count       = 0;
	from->count_pages = 0;
	from->size        = 0;
	from->lsn_min     = UINT64_MAX;
	from->lsn_max     = 0;
	list_init(&from->iterators);
}

hot static inline
rbtree_get(memtable_find, memtable_compare(arg, memtable_page_of(n), key))

hot static inline MemtablePage*
memtable_search_page(Memtable* self, Row* key)
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
memtable_search(Memtable* self, MemtablePage* page, Row* key, bool* match)
{
	int min = 0;
	int mid = 0;
	int max = page->rows_count - 1;

	if (compare(self->comparator, page->rows[max], key) < 0)
		return max + 1;

	while (max >= min)
	{
		mid = min + ((max - min) >> 1);
		int rc = compare(self->comparator, page->rows[mid], key);
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
	int pos = page->rows_count - 1;
	while (pos >= 0)
	{
		int rc = self->compare(self, page->rows[pos], key);
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
	int pos = range_of(self, tree)->rows_count - 1;
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

hot static inline Row*
memtable_insert(Memtable*      self,
                MemtablePage*  page,
                MemtablePage** page_split,
                Row*           row)
{
	bool match = false;
	int pos = memtable_search(self, page, row, &match);
	if (match)
	{
		// replace
		auto prev = page->rows[pos];
		page->rows[pos] = row;
		self->size -= row_size(prev);
		self->size += row_size(row);

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
	if (unlikely(page->rows_count == self->size_page))
	{
		r = memtable_page_allocate(self);
		l->rows_count = self->size_split;
		r->rows_count = self->size_page - self->size_split;
		memmove(&r->rows[0], &l->rows[self->size_split], sizeof(Row*) * r->rows_count);
		if (pos >= l->rows_count)
		{
			ref = r;
			pos = pos - l->rows_count;
		}
	}

	// insert
	int size = (ref->rows_count - pos) * sizeof(Row*);
	if (size > 0)
		memmove(&ref->rows[pos + 1], &ref->rows[pos], size);
	ref->rows[pos] = row;
	ref->rows_count++;
	self->count++;
	self->size += row_size(row);

	// update iterators
	if (self->iterators_count > 0)
		memtable_sync(self, l, r, pos);

	*page_split = r;
	return NULL;
}

hot Row*
memtable_set(Memtable* self, Row* row)
{
	// create root page
	if (self->count_pages == 0)
	{
		auto page = memtable_page_allocate(self);
		page->rows[0] = row;
		page->rows_count++;
		rbtree_set(&self->tree, NULL, 0, &page->node);
		self->count_pages++;
		self->count++;
		self->size += row_size(row);
		return NULL;
	}

	// search page
	auto page = memtable_search_page(self, row);

	// insert into page
	MemtablePage* page_split = NULL;
	auto prev = memtable_insert(self, page, &page_split, row);
	if (page_split)
	{
		// update split page
		rbtree_set(&self->tree, &page->node, -1, &page_split->node);
		self->count_pages++;
	}
	return prev;
}

hot bool
memtable_seek(Memtable* self, Row* key, MemtablePage** page, int* pos)
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
