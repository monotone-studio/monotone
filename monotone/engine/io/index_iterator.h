#pragma once

//
// monotone
//
// time-series storage
//

typedef struct IndexIterator IndexIterator;

struct IndexIterator
{
	Index*       index;
	IndexRegion* current;
	int          pos;
	Comparator*  comparator;
};

static inline void
index_iterator_init(IndexIterator* self)
{
	self->index      = NULL;
	self->current    = NULL;
	self->pos        = 0;
	self->comparator = NULL;
}

hot static inline int
index_iterator_search(IndexIterator* self, Row* row)
{
	int begin = 0;
	int end   = self->index->count - 1;
	while (begin != end)
	{
		int mid = begin + (end - begin) / 2;
		if (compare(self->comparator, index_region_max(self->index, mid), row) < 0)
			begin = mid + 1;
		else
			end = mid;
	}
	if (unlikely(end >= (int)self->index->count))
		end = self->index->count - 1;
	return end;
}

hot static inline void
index_iterator_open(IndexIterator* self,
                    Index*         index,
                    Comparator*    comparator,
                    Row*           row)
{
	self->index      = index;
	self->current    = NULL;
	self->pos        = 0;
	self->comparator = comparator;
	if (unlikely(index == NULL || index->count == 0))
		return;

	if (row == NULL) {
		self->current = index_get(index, 0);
		return;
	}

	self->pos = index_iterator_search(self, row);
	int rc;
	rc = compare(comparator, index_region_max(index, self->pos), row);
	if (rc < 0)
		self->pos++;
	if (unlikely(self->pos >= (int)index->count))
		return;

	self->current = index_get(index, self->pos);
}

static inline bool
index_iterator_has(IndexIterator* self)
{
	return self->current != NULL;
}

static inline IndexRegion*
index_iterator_at(IndexIterator* self)
{
	return self->current;
}

static inline void
index_iterator_next(IndexIterator* self)
{
	self->pos++;
	self->current = NULL;
	if (unlikely(self->pos >= (int)self->index->count))
		return;
	self->current = index_get(self->index, self->pos);
}
