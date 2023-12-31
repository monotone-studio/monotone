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
	Buf            read_buf;
	Buf            read_buf_uncompressed;
	Reader         reader;
};

hot static inline bool
part_iterator_open_region(PartIterator* self, Row* row)
{
	// read region from file
	Reader* reader = &self->reader;
	reader->index                 = self->part->index;
	reader->index_region          = self->current;
	reader->region                = NULL;
	reader->read_buf              = &self->read_buf;
	reader->read_buf_uncompressed = &self->read_buf_uncompressed;
	reader->mmap                  = &self->part->mmap;
	reader->file                  = &self->part->file;
	reader_execute(reader);

	// prepare region iterator
	region_iterator_reset(&self->region_iterator);
	return region_iterator_open(&self->region_iterator, reader->region,
	                             self->part->comparator,
	                             row);
}

hot static inline bool
part_iterator_open(PartIterator* self, Part* part, Row* row)
{
	self->part    = part;
	self->current = NULL;

	region_iterator_init(&self->region_iterator);
	index_iterator_init(&self->index_iterator);
	index_iterator_open(&self->index_iterator, part->index, part->comparator, row);

	self->current = index_iterator_at(&self->index_iterator);
	if (self->current == NULL)
		return false;

	return part_iterator_open_region(self, row);
}

hot static inline bool
part_iterator_has(PartIterator* self)
{
	return region_iterator_has(&self->region_iterator);
}

hot static inline Row*
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
	buf_free(&self->read_buf);
	buf_free(&self->read_buf_uncompressed);
}

static inline void
part_iterator_reset(PartIterator* self)
{
	reader_reset(&self->reader);
	buf_reset(&self->read_buf);
	buf_reset(&self->read_buf_uncompressed);
	region_iterator_reset(&self->region_iterator);
}

static inline void
part_iterator_init(PartIterator* self)
{
	self->part    = NULL;
	self->current = NULL;
	reader_init(&self->reader);
	buf_init(&self->read_buf);
	buf_init(&self->read_buf_uncompressed);
	index_iterator_init(&self->index_iterator);
	region_iterator_init(&self->region_iterator);

	auto it = &self->iterator;
	it->has   = (IteratorHas)part_iterator_has;
	it->at    = (IteratorAt)part_iterator_at;
	it->next  = (IteratorNext)part_iterator_next;
	it->close = (IteratorClose)part_iterator_free;
}