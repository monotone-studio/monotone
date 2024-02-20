#pragma once

//
// monotone
//
// time-series storage
//

typedef struct RegionIterator RegionIterator;

struct RegionIterator
{
	Iterator    iterator;
	Region*     region;
	int         pos;
	Event*      current;
	Comparator* comparator;
};

hot static inline void
region_iterator_set_pos(RegionIterator* self, int pos)
{
	if (unlikely(pos < 0 || pos >= (int)self->region->events))
	{
		self->current = NULL;
		self->pos = 0;
		return;
	}
	self->current = region_get(self->region, pos);
	self->pos = pos;
}

always_inline hot static inline int
region_iterator_compare(RegionIterator* self, int pos, Event* event)
{
	auto current = region_get(self->region, pos);
	return compare(self->comparator, current, event);
}

hot static inline int
region_iterator_search(RegionIterator* self, Event* event, bool* match)
{
	int min = 0;
	int mid = 0;
	int max = self->region->events - 1;
	while (max >= min)
	{
		mid = min + (max - min) / 2;
		int rc = region_iterator_compare(self, mid, event);
		if (rc < 0) {
			min = mid + 1;
		} else
		if (rc > 0) {
			max = mid - 1;
		} else {
			*match = true;
		    return mid;
		}
	}
	return min;
}

hot static inline bool
region_iterator_open(RegionIterator* self,
                     Region*         region,
                     Comparator*     comparator,
                     Event*          event)
{
	self->region     = region;
	self->pos        = 0;
	self->current    = NULL;
	self->comparator = comparator;
	if (unlikely(self->region->events == 0))
		return false;
	bool match = false;
	int pos;
	if (event)
		pos = region_iterator_search(self, event, &match);
	else
		pos = 0;
	region_iterator_set_pos(self, pos);
	return match;
}

static inline bool
region_iterator_has(RegionIterator* self)
{
	return self->current != NULL;
}

static inline Event*
region_iterator_at(RegionIterator* self)
{
	return self->current;
}

hot static inline void
region_iterator_next(RegionIterator* self)
{
	if (unlikely(self->current == NULL))
		return;
	region_iterator_set_pos(self, self->pos + 1);
}

static inline void
region_iterator_init(RegionIterator* self)
{
	self->current    = NULL;
	self->pos        = 0;
	self->region     = NULL;
	self->comparator = NULL;
	auto it = &self->iterator;
	it->has   = (IteratorHas)region_iterator_has;
	it->at    = (IteratorAt)region_iterator_at;
	it->next  = (IteratorNext)region_iterator_next;
	it->close = NULL;
}

static inline void
region_iterator_reset(RegionIterator* self)
{
	self->current    = NULL;
	self->pos        = 0;
	self->region     = NULL;
	self->comparator = NULL;
}
