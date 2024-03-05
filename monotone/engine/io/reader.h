#pragma once

//
// monotone
//
// time-series storage
//

typedef struct Reader Reader;

struct Reader
{
	Part*        part;
	Compression* compression;
	Buf          buf;
	Buf          buf_uncompressed;
};

static inline void
reader_init(Reader* self)
{
	self->part = NULL;
	self->compression = NULL;
	buf_init(&self->buf);
	buf_init(&self->buf_uncompressed);
}

static inline void
reader_reset(Reader* self)
{
	if (self->compression)
	{
		compression_mgr_push(global()->compression_mgr, self->compression);
		self->compression = NULL;
	}
	buf_reset(&self->buf);
	buf_reset(&self->buf_uncompressed);
}

static inline void
reader_free(Reader* self)
{
	buf_free(&self->buf);
	buf_free(&self->buf_uncompressed);
}

static inline void
reader_open(Reader* self, Part* part)
{
	self->part = part;

	// set compression context using partition index
	int compression_id = part->index->compression;
	if (compression_id != COMPRESSION_NONE)
		self->compression =
			compression_mgr_pop(global()->compression_mgr, compression_id);
}

hot static inline Region*
reader_execute(Reader* self, IndexRegion* index_region)
{
	auto part = self->part;

	buf_reset(&self->buf);
	buf_reset(&self->buf_uncompressed);

	if (part_has(part, PART))
	{
		// read region data from file
		file_pread_buf(&part->file, &self->buf, index_region->size,
		               index_region->offset);
	} else
	if (part_has(part, PART_CLOUD))
	{
		// read region data from cloud
		cloud_read(part->cloud, part->source, &part->id,
		           &self->buf,
		           index_region->size,
		           index_region->offset);
	} else {
		abort();
	}
	auto region = (Region*)self->buf.start;

	// decompress region
	if (self->compression)
	{
		compression_decompress(self->compression,
		                       &self->buf_uncompressed,
		                       (uint8_t*)region,
		                       index_region->size,
		                       index_region->size_origin);
		region = (Region*)self->buf_uncompressed.start;
	}

	return region;
}
