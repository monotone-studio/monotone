
//
// noire
//
// time-series storage
//

#include <noire_runtime.h>
#include <noire_lib.h>
#include <noire_io.h>

void
part_writer_init(PartWriter* self)
{
	self->part = NULL;
	self->storage = NULL;
	iov_init(&self->iov);
	region_writer_init(&self->region_writer);
	index_writer_init(&self->index_writer);
}

void
part_writer_free(PartWriter* self)
{
	if (self->part)
		part_free(self->part);
	iov_free(&self->iov);
	region_writer_free(&self->region_writer);
	index_writer_free(&self->index_writer);
}

void
part_writer_reset(PartWriter* self)
{
	self->storage = NULL;
	if (self->part)
	{
		part_free(self->part);
		self->part = NULL;
	}
	iov_reset(&self->iov);
	region_writer_reset(&self->region_writer);
	index_writer_reset(&self->index_writer);
}

hot static inline bool
part_writer_is_region_limit(PartWriter* self)
{
	if (unlikely(! region_writer_started(&self->region_writer)))
		return true;
	return region_writer_size(&self->region_writer) >= (uint32_t)self->storage->region_size;
}

static inline void
part_writer_start_region(PartWriter* self)
{
	region_writer_reset(&self->region_writer);
	region_writer_start(&self->region_writer, self->storage->compression);
}

hot static inline void
part_writer_stop_region(PartWriter* self)
{
	if (! region_writer_started(&self->region_writer))
		return;

	// complete region
	region_writer_stop(&self->region_writer);

	// add region to the index
	index_writer_add(&self->index_writer, &self->region_writer,
	                  blob_size(&self->part->mmap));

	// write region
	iov_reset(&self->iov);
	region_writer_add_to_iov(&self->region_writer, &self->iov);
	blob_writev(&self->part->mmap, iov_pointer(&self->iov), self->iov.iov_count);
}

void
part_writer_start(PartWriter* self,
                  Comparator* comparator,
                  Storage*    storage,
                  uint64_t    min,
                  uint64_t    max)
{
	// create new partition
	self->storage = storage;
	self->part = part_allocate(comparator, storage, min, max);

	// start new index
	index_writer_reset(&self->index_writer);
	index_writer_start(&self->index_writer, storage->compression, storage->crc);
}

void
part_writer_stop(PartWriter* self, uint64_t lsn)
{
	if (! index_writer_started(&self->index_writer))
		return;

	if (region_writer_started(&self->region_writer))
		part_writer_stop_region(self);

	index_writer_stop(&self->index_writer, lsn);

	// copy index to the partition
	index_writer_copy(&self->index_writer, &self->part->index_buf);
	self->part->index = (Index*)(self->part->index_buf.start);
}

void
part_writer_add(PartWriter* self, Row* row)
{
	if (unlikely(part_writer_is_region_limit(self)))
	{
		// write region
		part_writer_stop_region(self);

		// start new region
		part_writer_start_region(self);
	}

	// add next row to the region
	region_writer_add(&self->region_writer, row);
}
