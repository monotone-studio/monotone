#pragma once

//
// monotone
//
// time-series storage
//

typedef struct MemtableRow MemtableRow;
typedef struct Memtable    Memtable;

struct MemtableRow
{
	RbtreeNode node;
	Row        row;
};

struct Memtable
{
	uint64_t    count;
	uint64_t    size;
	uint64_t    lsn_min;
	uint64_t    lsn_max;
	Rbtree      index;
	Comparator* comparator;
};

hot static inline MemtableRow*
memtable_row_allocate(uint64_t time, uint8_t* data, int data_size)
{
	MemtableRow* ref;
	ref = nr_malloc(sizeof(MemtableRow) + data_size);
	row_init(&ref->row, time, data, data_size);
	rbtree_init_node(&ref->node);
	return ref;
}

always_inline hot static inline void
memtable_row_free(MemtableRow* self)
{
	nr_free(self);
}

always_inline static inline MemtableRow*
memtable_row_of(RbtreeNode* node)
{
	return container_of(node, MemtableRow, node);
}

always_inline hot static inline int
memtable_compare(Memtable* self, MemtableRow* a, MemtableRow* b)
{
	return compare(self->comparator, &a->row, &b->row);
}

static inline void
memtable_init(Memtable* self, Comparator* comparator)
{
	self->count      = 0;
	self->size       = 0;
	self->comparator = comparator;
	rbtree_init(&self->index);
}

rbtree_free(memtable_truncate, memtable_row_free(memtable_row_of(n)))

static inline void
memtable_free(Memtable* self)
{
	if (self->index.root)
		memtable_truncate(self->index.root, NULL);
}

hot static inline void
memtable_follow_lsn(Memtable* self, uint64_t lsn)
{
	if (lsn < self->lsn_min)
		self->lsn_min = lsn;
	if (lsn > self->lsn_max)
		self->lsn_max = lsn;
}

hot static inline
rbtree_get(memtable_match, memtable_compare(arg, memtable_row_of(n), key))

hot static inline void
memtable_set(Memtable* self, MemtableRow* row)
{
	RbtreeNode* node;
	int rc;
	rc = memtable_match(&self->index, self->comparator, row, &node);
	if (rc == 0 && node)
	{
		// replace
		auto head = memtable_row_of(node);
		rbtree_replace(&self->index, &head->node, &row->node);
		self->size -= row_size(&row->row) + sizeof(MemtableRow);
		self->count--;
	} else
	{
		// insert
		rbtree_set(&self->index, node, rc, &row->node);
	}
	self->size += row_size(&row->row) + sizeof(MemtableRow);
	self->count++;
}
