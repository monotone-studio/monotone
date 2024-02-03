#pragma once

//
// monotone
//
// time-series storage
//

typedef struct Reader Reader;

struct Reader
{
	Buf buf;
	Buf buf_uncompressed;
};

static inline void
reader_init(Reader* self)
{
	buf_init(&self->buf);
	buf_init(&self->buf_uncompressed);
}

static inline void
reader_reset(Reader* self)
{
	buf_reset(&self->buf);
	buf_reset(&self->buf_uncompressed);
}

static inline void
reader_free(Reader* self)
{
	buf_free(&self->buf);
	buf_free(&self->buf_uncompressed);
}

hot static inline Region*
reader_execute(Reader* self, Part* part, IndexRegion* index_region)
{
	buf_reset(&self->buf);
	buf_reset(&self->buf_uncompressed);

	if (part_has(part, PART_FILE))
	{
		// read region data from file
		file_pread_buf(&part->file, &self->buf, index_region->size,
		               index_region->offset);
	} else
	if (part_has(part, PART_FILE_CLOUD))
	{
		// read region data from cloud
		cloud_read(part->cloud, &part->id,
		           &self->buf,
		           index_region->size,
		           index_region->offset);
	} else {
		abort();
	}
	auto region = (Region*)self->buf.start;

	// decompress region
	int compression = part->index->compression;
	if (compression_is_set(compression))
	{
		compression_decompress(&self->buf_uncompressed,
		                       compression,
		                       index_region->size_origin,
		                       (char*)region,
		                       index_region->size);
		region = (Region*)self->buf_uncompressed.start;
	}

	return region;
}
