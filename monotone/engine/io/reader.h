#pragma once

//
// monotone
//
// time-series storage
//

typedef struct Reader Reader;

struct Reader
{
	Index*       index;
	IndexRegion* index_region;
	Region*      region;
	Buf*         read_buf;
	Buf*         read_buf_uncompressed;
	File*        file;
	Cloud*       cloud;
};

static inline void
reader_init(Reader* self)
{
	self->index                 = NULL;
	self->index_region          = NULL;
	self->region                = NULL;
	self->read_buf              = NULL;
	self->read_buf_uncompressed = NULL;
	self->file                  = NULL;
	self->cloud                 = NULL;
}

static inline void
reader_reset(Reader* self)
{
	self->index                 = NULL;
	self->index_region          = NULL;
	self->region                = NULL;
	self->read_buf              = NULL;
	self->read_buf_uncompressed = NULL;
	self->file                  = NULL;
	self->cloud                 = NULL;
}

hot static inline void
reader_execute(Reader* self)
{
	buf_reset(self->read_buf);
	buf_reset(self->read_buf_uncompressed);

	if (file_is_openned(self->file))
	{
		// read region data from file
		file_pread_buf(self->file, self->read_buf, self->index_region->size,
		               self->index_region->offset);
	} else
	{
		if (! self->cloud)
			error("partition file not found");

		// read region data from cloud
		cloud_read(self->cloud, self->index->id_min, self->index->id,
		           self->read_buf,
		           self->index_region->size,
		           self->index_region->offset);
	}
	self->region = (Region*)self->read_buf->start;

	// decompress region
	int compression = self->index->compression;
	if (compression_is_set(compression))
	{
		compression_decompress(self->read_buf_uncompressed,
		                       compression,
		                       self->index_region->size_origin,
		                       (char*)self->region,
		                       self->index_region->size);
		self->region = (Region*)self->read_buf_uncompressed->start;
	}
}
