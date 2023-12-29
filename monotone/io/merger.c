
//
// monotone
//
// time-series storage
//

#include <monotone_runtime.h>
#include <monotone_lib.h>
#include <monotone_io.h>

void
merger_init(Merger* self)
{
	part_iterator_init(&self->part_iterator);
	memtable_iterator_init(&self->memtable_iterator);
	merge_iterator_init(&self->merge_iterator);
	part_writer_init(&self->writer);
}

void
merger_free(Merger* self)
{
	part_iterator_free(&self->part_iterator);
	merge_iterator_free(&self->merge_iterator);
	part_writer_free(&self->writer);
}

void
merger_reset(Merger* self)
{
	part_iterator_reset(&self->part_iterator);
	memtable_iterator_reset(&self->memtable_iterator);
	merge_iterator_reset(&self->merge_iterator);
	part_writer_reset(&self->writer);
}

hot static inline void
merger_main(Merger* self, MergerReq* req)
{
	auto writer = &self->writer;
	auto it = &self->merge_iterator;
	auto origin = req->origin;
	auto memtable = req->memtable;

	part_writer_start(writer, origin->comparator, req->storage,
	                  origin->min,
	                  origin->max);
	for (;;)
	{
		auto row = merge_iterator_at(it);
		if (unlikely(row == NULL))
			break;
		if (row->is_delete)
		{
			merge_iterator_next(it);
			continue;
		}
		part_writer_add(writer, row);
		merge_iterator_next(it);
	}

	uint64_t lsn_max = memtable->lsn_max;
	if (origin->index && origin->index->lsn_max > lsn_max)
		lsn_max = origin->index->lsn_max;

	part_writer_stop(writer, lsn_max);
}

hot Part*
merger_execute(Merger* self, MergerReq* req)
{
	// prepare memtable iterator
	memtable_iterator_open(&self->memtable_iterator, req->memtable, NULL);

	// prepare partition iterator
	part_iterator_reset(&self->part_iterator);
	part_iterator_open(&self->part_iterator, req->origin, NULL);

	// prepare merge iterator
	auto it = &self->merge_iterator;
	merge_iterator_reset(it);
	merge_iterator_add(it, &self->memtable_iterator.iterator);
	merge_iterator_add(it, &self->part_iterator.iterator);
	merge_iterator_open(it, req->origin->comparator);

	// create new partition (in-memory)
	merger_main(self, req);

	// create incomplete partition file
	auto part = self->writer.part;
	/*part_create(part);*/
	/*part_unload(part);*/

	// return partition
	self->writer.part = NULL;
	return part;
}
