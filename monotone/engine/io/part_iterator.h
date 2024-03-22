#pragma once

//
// monotone
//
// time-series storage
//

typedef struct PartIterator PartIterator;

struct PartIterator
{
	Iterator       iterator;
	RegionIterator region_iterator;
	IndexIterator  index_iterator;
	IndexRegion*   current;
	Part*          part;
	Reader         reader;
};

hot static inline bool
part_iterator_open_region(PartIterator* self, Event* event)
{
	// read region from file
	auto region = reader_execute(&self->reader, self->current);

	// prepare region iterator
	region_iterator_reset(&self->region_iterator);
	return region_iterator_open(&self->region_iterator, region,
	                             self->part->comparator,
	                             event);
}

hot static inline bool
part_iterator_open(PartIterator* self, Part* part, Event* event)
{
	self->part    = part;
	self->current = NULL;

	region_iterator_init(&self->region_iterator);
	index_iterator_init(&self->index_iterator);
	index_iterator_open(&self->index_iterator, &part->index, &part->index_data, event);

	self->current = index_iterator_at(&self->index_iterator);
	if (self->current == NULL)
		return false;

	reader_open(&self->reader, part);
	return part_iterator_open_region(self, event);
}

hot static inline bool
part_iterator_has(PartIterator* self)
{
	return region_iterator_has(&self->region_iterator);
}

hot static inline Event*
part_iterator_at(PartIterator* self)
{
	return region_iterator_at(&self->region_iterator);
}

hot static inline void
part_iterator_next(PartIterator* self)
{
	if (unlikely(self->current == NULL))
		return;

	// iterate over current region
	region_iterator_next(&self->region_iterator);

	for (;;)
	{
		if (likely(region_iterator_has(&self->region_iterator)))
			break;

		// read next region
		index_iterator_next(&self->index_iterator);
		self->current = index_iterator_at(&self->index_iterator);
		if (unlikely(self->current == NULL))
			break;

		part_iterator_open_region(self, NULL);
	}
}

static inline void
part_iterator_free(PartIterator* self)
{
	reader_reset(&self->reader);
	reader_free(&self->reader);
}

static inline void
part_iterator_reset(PartIterator* self)
{
	reader_reset(&self->reader);
	region_iterator_reset(&self->region_iterator);
}

static inline void
part_iterator_init(PartIterator* self)
{
	self->part    = NULL;
	self->current = NULL;
	reader_init(&self->reader);
	index_iterator_init(&self->index_iterator);
	region_iterator_init(&self->region_iterator);

	auto it = &self->iterator;
	it->has   = (IteratorHas)part_iterator_has;
	it->at    = (IteratorAt)part_iterator_at;
	it->next  = (IteratorNext)part_iterator_next;
	it->close = (IteratorClose)part_iterator_free;
}
