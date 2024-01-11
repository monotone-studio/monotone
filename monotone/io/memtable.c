
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
	self->count       = 0;
	self->count_pages = 0;
	self->size        = 0;
	self->size_page   = size_page;
	self->size_split  = size_split;
	self->comparator  = comparator;
	self->lsn_min     = UINT64_MAX;
	self->lsn_max     = 0;
	rbtree_init(&self->tree);
}

static MemtablePage*
memtable_page_allocate(Memtable* self)
{
	MemtablePage* page;
	page = mn_malloc(sizeof(MemtablePage) + self->size_page * sizeof(Row*));
	page->rows_count = 0;
	rbtree_init_node(&page->node);
	return page;
}

static inline void
memtable_page_free(MemtablePage* page)
{
	for (int i = 0; i< page->rows_count; i++)
		row_free(page->rows[i]);
	mn_free(page);
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

rbtree_free(memtable_truncate, memtable_page_free(memtable_page_of(n)))

void
memtable_free(Memtable* self)
{
	if (self->tree.root)
		memtable_truncate(self->tree.root, NULL);
	self->count       = 0;
	self->count_pages = 0;
	self->size        = 0;
	self->lsn_min     = UINT64_MAX;
	self->lsn_max     = 0;
	rbtree_init(&self->tree);
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

hot static inline MemtablePage*
memtable_insert(Memtable* self, MemtablePage* page, Row* row)
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
		row_free(prev);
		return NULL;
	}
	if (pos < 0)
		pos = 0;

	// split
	MemtablePage* ref = page;
	MemtablePage* r = NULL;
	if (unlikely(page->rows_count == self->size_page))
	{
		auto l = page;
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
	return r;
}

hot void
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
		return;
	}

	// search page
	auto page = memtable_search_page(self, row);

	// insert into page
	auto split = memtable_insert(self, page, row);
	if (split)
	{
		// update split page
		rbtree_set(&self->tree, &page->node, -1, &split->node);
		self->count_pages++;
	}
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
	if (lsn < self->lsn_min)
		self->lsn_min = lsn;
	if (lsn > self->lsn_max)
		self->lsn_max = lsn;
}
