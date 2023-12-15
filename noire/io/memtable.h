#pragma once

//
// noire
//
// time-series storage
//

typedef struct MemtableRow MemtableRow;
typedef struct Memtable    Memtable;

struct MemtableRow
{
	RbtreeNode node;
	Row*       row;
};

struct Memtable
{
	uint64_t    count;
	uint64_t    size;
	Rbtree      index;
	Comparator* comparator;
};

hot static inline MemtableRow*
memtable_row_allocate(Row* row)
{
	MemtableRow* ref;
	ref = nr_malloc(sizeof(MemtableRow));
	ref->row = row;
	rbtree_init_node(&ref->node);
	return ref;
}

hot static inline void
memtable_row_free(MemtableRow* self)
{
	if (self->row)
		row_free(self->row);
	nr_free(self);
}

always_inline static inline MemtableRow*
memtable_row_of(RbtreeNode* node)
{
	return container_of(node, MemtableRow, node);
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

hot static inline
rbtree_get(memtable_match, compare(arg, memtable_row_of(n)->row, key))

hot static inline bool
memtable_insert(Memtable* self, MemtableRow* update)
{
	RbtreeNode* node;
	int rc;
	rc = memtable_match(&self->index, self->comparator, update->row, &node);
	if (unlikely(rc == 0 && node))
		return true;
	rbtree_set(&self->index, node, rc, &update->node);
	self->size += row_size(update->row) + sizeof(MemtableRow);
	self->count++;
	return false;
}

hot static inline void
memtable_replace(Memtable* self, Row* row)
{
	RbtreeNode* node;
	int rc;
	rc = memtable_match(&self->index, self->comparator, row, &node);
	if (rc == 0 && node)
	{
		// replace
		auto head = memtable_row_of(node);
		auto update = memtable_row_allocate(row);
		rbtree_replace(&self->index, &head->node, &update->node);
		self->size -= row_size(row) + sizeof(MemtableRow);
		self->count--;
	} else
	{
		// insert
		auto update = memtable_row_allocate(row);
		rbtree_set(&self->index, node, rc, &update->node);
	}
	self->size += row_size(row) + sizeof(MemtableRow);
	self->count++;
}
