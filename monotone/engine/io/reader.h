#pragma once

//
// monotone
//
// time-series storage
//

typedef struct Reader Reader;

struct Reader
{
	Id*          id;
	Index*       index;
	IndexRegion* index_region;
	Region*      region;
	File*        file;
	Cloud*       cloud;
	Buf          buf;
	Buf          buf_uncompressed;
};

static inline void
reader_init(Reader* self)
{
	self->id           = NULL;
	self->index        = NULL;
	self->index_region = NULL;
	self->region       = NULL;
	self->file         = NULL;
	self->cloud        = NULL;
	buf_init(&self->buf);
	buf_init(&self->buf_uncompressed);
}

static inline void
reader_reset(Reader* self)
{
	self->id           = NULL;
	self->index        = NULL;
	self->index_region = NULL;
	self->region       = NULL;
	self->file         = NULL;
	self->cloud        = NULL;
	buf_reset(&self->buf);
	buf_reset(&self->buf_uncompressed);
}

static inline void
reader_free(Reader* self)
{
	buf_free(&self->buf);
	buf_free(&self->buf_uncompressed);
}

hot static inline void
reader_execute(Reader* self)
{
	buf_reset(&self->buf);
	buf_reset(&self->buf_uncompressed);

	if (file_is_openned(self->file))
	{
		// read region data from file
		file_pread_buf(self->file, &self->buf, self->index_region->size,
		               self->index_region->offset);
	} else
	{
		if (! self->cloud)
			error("partition file not found");

		// read region data from cloud
		cloud_read(self->cloud, self->id,
		           &self->buf,
		           self->index_region->size,
		           self->index_region->offset);
	}
	self->region = (Region*)self->buf.start;

	// decompress region
	int compression = self->index->compression;
	if (compression_is_set(compression))
	{
		compression_decompress(&self->buf_uncompressed,
		                       compression,
		                       self->index_region->size_origin,
		                       (char*)self->region,
		                       self->index_region->size);
		self->region = (Region*)self->buf_uncompressed.start;
	}
}
