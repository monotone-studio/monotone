#pragma once

//
// monotone
//
// time-series storage
//

typedef struct PartCursor PartCursor;

struct PartCursor
{
	Part*            part;
	PartIterator     part_iterator;
	MemtableIterator memtable_iterator_a;
	MemtableIterator memtable_iterator_b;
	MergeIterator    merge_iterator;
};

hot static inline void
part_cursor_open(PartCursor* self, Part* part, Row* row)
{
	self->part = part;

	// open memtable iterators
	Memtable* memtable_prev;
	auto memtable = part_memtable(part, &memtable_prev);
	if (memtable->count > 0)
	{
		memtable_iterator_open(&self->memtable_iterator_a, memtable, row, true);
		merge_iterator_add(&self->merge_iterator, &self->memtable_iterator_a.iterator);
	}
	if (memtable_prev->count > 0)
	{
		memtable_iterator_open(&self->memtable_iterator_b, memtable_prev, row, false);
		merge_iterator_add(&self->merge_iterator, &self->memtable_iterator_b.iterator);
	}

	// open part iterator
	part_iterator_open(&self->part_iterator, part, row);
	merge_iterator_add(&self->merge_iterator, &self->part_iterator.iterator);

	// open merge iterator
	merge_iterator_open(&self->merge_iterator, part->comparator);
}

hot static inline bool
part_cursor_has(PartCursor* self)
{
	return merge_iterator_has(&self->merge_iterator);
}

hot static inline Row*
part_cursor_at(PartCursor* self)
{
	return merge_iterator_at(&self->merge_iterator);
}

hot static inline void
part_cursor_next(PartCursor* self)
{
	merge_iterator_next(&self->merge_iterator);
}

static inline void
part_cursor_init(PartCursor* self)
{
	self->part = NULL;
	part_iterator_init(&self->part_iterator);
	memtable_iterator_init(&self->memtable_iterator_a);
	memtable_iterator_init(&self->memtable_iterator_b);
	merge_iterator_init(&self->merge_iterator);
}

static inline void
part_cursor_reset(PartCursor* self)
{
	self->part = NULL;
	part_iterator_reset(&self->part_iterator);
	memtable_iterator_init(&self->memtable_iterator_a);
	memtable_iterator_init(&self->memtable_iterator_b);
	merge_iterator_reset(&self->merge_iterator);
}

static inline void
part_cursor_close(PartCursor* self)
{
	memtable_iterator_close(&self->memtable_iterator_a);
	memtable_iterator_close(&self->memtable_iterator_b);
	part_cursor_reset(self);
}

static inline void
part_cursor_free(PartCursor* self)
{
	self->part = NULL;
	part_iterator_free(&self->part_iterator);
	merge_iterator_free(&self->merge_iterator);
}
