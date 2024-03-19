
//
// monotone
//
// time-series storage
//

#include <monotone_runtime.h>
#include <monotone_lib.h>
#include <monotone_config.h>
#include <monotone_cloud.h>
#include <monotone_io.h>

void
merger_init(Merger* self)
{
	self->part = NULL;
	part_iterator_init(&self->part_iterator);
	memtable_iterator_init(&self->memtable_iterator);
	merge_iterator_init(&self->merge_iterator);
	writer_init(&self->writer);
}

void
merger_free(Merger* self)
{
	if (self->part)
	{
		part_free(self->part);
		self->part = NULL;
	}
	part_iterator_free(&self->part_iterator);
	merge_iterator_free(&self->merge_iterator);
	writer_free(&self->writer);
}

void
merger_reset(Merger* self)
{
	if (self->part)
	{
		part_free(self->part);
		self->part = NULL;
	}
	part_iterator_reset(&self->part_iterator);
	memtable_iterator_reset(&self->memtable_iterator);
	merge_iterator_reset(&self->merge_iterator);
	writer_reset(&self->writer);
}

hot static inline void
merger_write(Merger* self, MergerReq* req)
{
	auto writer   = &self->writer;
	auto it       = &self->merge_iterator;
	auto origin   = req->origin;
	auto memtable = req->memtable;

	writer_start(writer, req->source, &self->part->file);
	for (;;)
	{
		auto event = merge_iterator_at(it);
		if (unlikely(event == NULL))
			break;
		if (event->is_delete)
		{
			merge_iterator_next(it);
			continue;
		}
		writer_add(writer, event);
		merge_iterator_next(it);
	}

	uint64_t refreshes = 1;
	uint64_t lsn = memtable->lsn_max;
	if (origin->state != ID_NONE)
	{
		refreshes += origin->index.refreshes;
		if (origin->index.lsn > lsn)
			lsn = origin->index.lsn;
	}

	auto id = &self->part->id;
	writer_stop(writer, id, refreshes, self->part->time, lsn,
	            req->source->sync);

	// copy index to the partition
	index_writer_copy(&self->writer.index_writer,
	                  &self->part->index,
	                  &self->part->index_data);
}

hot void
merger_execute(Merger* self, MergerReq* req)
{
	auto origin = req->origin;

	// prepare memtable iterator
	memtable_iterator_open(&self->memtable_iterator, req->memtable, NULL, false);

	// prepare partition iterator
	part_iterator_reset(&self->part_iterator);
	part_iterator_open(&self->part_iterator, origin, NULL);

	// prepare merge iterator
	auto it = &self->merge_iterator;
	merge_iterator_reset(it);
	merge_iterator_add(it, &self->memtable_iterator.iterator);
	merge_iterator_add(it, &self->part_iterator.iterator);
	merge_iterator_open(it, origin->comparator);

	// allocate and create incomplete partition file
	Id id = origin->id;
	self->part = part_allocate(origin->comparator, req->source, &id);
	part_create(self->part, ID_INCOMPLETE);
	part_set(self->part, ID_INCOMPLETE);
	part_set_time(self->part, time_us());

	// write partition file
	merger_write(self, req);
}
