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
	Buf*         index_data;
	IndexRegion* current;
	int          pos;
};

static inline void
index_iterator_init(IndexIterator* self)
{
	self->index      = NULL;
	self->index_data = NULL;
	self->current    = NULL;
	self->pos        = 0;
}

hot static inline int
index_iterator_search(IndexIterator* self, uint64_t id)
{
	int begin = 0;
	int end   = self->index->regions - 1;
	while (begin != end)
	{
		int mid = begin + (end - begin) / 2;
		auto at = index_region_max(self->index, self->index_data, mid);
		if (compare_u64(at, id) < 0)
			begin = mid + 1;
		else
			end = mid;
	}
	if (unlikely(end >= (int)self->index->regions))
		end = self->index->regions - 1;
	return end;
}

hot static inline void
index_iterator_open(IndexIterator* self,
                    Index*         index,
                    Buf*           index_data,
                    Event*         event)
{
	self->index      = index;
	self->index_data = index_data;
	self->current    = NULL;
	self->pos        = 0;
	if (unlikely(index == NULL || index->regions == 0))
		return;

	if (event == NULL)
	{
		self->current = index_get(index, index_data, 0);
		return;
	}

	self->pos = index_iterator_search(self, event->id);
	int rc = compare_u64(index_region_max(index, index_data, self->pos), event->id);
	if (rc < 0)
		self->pos++;
	if (unlikely(self->pos >= (int)index->regions))
		return;

	self->current = index_get(index, index_data, self->pos);
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
	if (unlikely(self->pos >= (int)self->index->regions))
		return;
	self->current = index_get(self->index, self->index_data, self->pos);
}
