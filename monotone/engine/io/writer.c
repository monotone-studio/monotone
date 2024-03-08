
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
writer_init(Writer* self)
{
	self->file        = NULL;
	self->compression = NULL;
	self->source      = NULL;
	iov_init(&self->iov);
	region_writer_init(&self->region_writer);
	index_writer_init(&self->index_writer);
}

void
writer_free(Writer* self)
{
	iov_free(&self->iov);
	region_writer_free(&self->region_writer);
	index_writer_free(&self->index_writer);
}

void
writer_reset(Writer* self)
{
	if (self->compression)
	{
		compression_mgr_push(global()->compression_mgr, self->compression);
		self->compression = NULL;
	}
	self->source = NULL;
	iov_reset(&self->iov);
	region_writer_reset(&self->region_writer);
	index_writer_reset(&self->index_writer);
}

hot static inline bool
writer_is_region_limit(Writer* self)
{
	if (unlikely(! region_writer_started(&self->region_writer)))
		return true;
	return region_writer_size(&self->region_writer) >= (uint32_t)self->source->region_size;
}

static inline void
writer_start_region(Writer* self)
{
	region_writer_reset(&self->region_writer);
	region_writer_start(&self->region_writer, self->compression,
	                    self->source->compression_level);
}

hot static inline void
writer_stop_region(Writer* self)
{
	if (! region_writer_started(&self->region_writer))
		return;

	// complete region
	region_writer_stop(&self->region_writer);

	// add region to the index
	index_writer_add(&self->index_writer, &self->region_writer,
	                  self->file->size);

	// write region
	iov_reset(&self->iov);
	region_writer_add_to_iov(&self->region_writer, &self->iov);
	file_writev(self->file, iov_pointer(&self->iov), self->iov.iov_count);
}

void
writer_start(Writer* self, Source* source, File* file)
{
	self->source = source;
	self->file   = file;

	// get compression context
	int compression_id = compression_mgr_of(&source->compression);
	if (compression_id != COMPRESSION_NONE)
		self->compression =
			compression_mgr_pop(global()->compression_mgr, compression_id);

	// start new index
	index_writer_reset(&self->index_writer);
	index_writer_start(&self->index_writer, self->compression,
	                    source->compression_level,
	                    source->crc);
}

void
writer_stop(Writer* self, Id* id, uint64_t time, uint64_t lsn, bool sync)
{
	if (! index_writer_started(&self->index_writer))
		return;

	if (region_writer_started(&self->region_writer))
		writer_stop_region(self);

	index_writer_stop(&self->index_writer, id, time, lsn);

	// write index
	iov_reset(&self->iov);
	index_writer_add_to_iov(&self->index_writer, &self->iov);
	file_writev(self->file, iov_pointer(&self->iov), self->iov.iov_count);
	
	// sync
	if (sync)
		file_sync(self->file);

	if (self->compression)
	{
		compression_mgr_push(global()->compression_mgr, self->compression);
		self->compression = NULL;
	}
}

void
writer_add(Writer* self, Event* event)
{
	if (unlikely(writer_is_region_limit(self)))
	{
		// write region
		writer_stop_region(self);

		// start new region
		writer_start_region(self);
	}

	// add next event to the region
	region_writer_add(&self->region_writer, event);
}
