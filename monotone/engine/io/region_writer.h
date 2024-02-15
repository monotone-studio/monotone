#pragma once

//
// monotone
//
// time-series storage
//

typedef struct RegionWriter RegionWriter;

struct RegionWriter
{
	Buf          meta;
	Buf          data;
	Buf          compressed;
	Compression* compression;
};

static inline void
region_writer_init(RegionWriter* self)
{
	self->compression = NULL;
	buf_init(&self->meta);
	buf_init(&self->data);
	buf_init(&self->compressed);
}

static inline void
region_writer_free(RegionWriter* self)
{
	buf_free(&self->meta);
	buf_free(&self->data);
	buf_free(&self->compressed);
}

static inline void
region_writer_reset(RegionWriter* self)
{
	self->compression = NULL;
	buf_reset(&self->meta);
	buf_reset(&self->data);
	buf_reset(&self->compressed);
}

static inline Region*
region_writer_header(RegionWriter* self)
{
	assert(self->meta.start != NULL);
	return (Region*)self->meta.start;
}

static inline bool
region_writer_started(RegionWriter* self)
{
	return buf_size(&self->meta) > 0;
}

static inline uint32_t
region_writer_size(RegionWriter* self)
{
	return buf_size(&self->meta) + buf_size(&self->data);
}

static inline void
region_writer_start(RegionWriter* self, Compression* compression)
{
	self->compression = compression;

	// region header
	buf_reserve(&self->meta, sizeof(Region));
	auto header = region_writer_header(self);
	memset(header, 0, sizeof(*header));
	buf_advance(&self->meta, sizeof(Region));
}

static inline void
region_writer_stop(RegionWriter* self)
{
	auto header = region_writer_header(self);
	header->size = region_writer_size(self);

	// compress region
	if (self->compression)
		compression_compress(self->compression, &self->compressed, 0,
		                     &self->meta, &self->data);
}

static inline void
region_writer_add(RegionWriter* self, Row* row)
{
	// write data offset
	uint32_t offset = buf_size(&self->data);
	buf_write(&self->meta, &offset, sizeof(offset));

	// copy data
	uint32_t size = row_size(row);
	buf_write(&self->data, row, size);

	// update header
	auto header = region_writer_header(self);
	header->rows++;
}

static inline Row*
region_writer_min(RegionWriter* self)
{
	return (Row*)self->data.start;
}

static inline Row*
region_writer_max(RegionWriter* self)
{
	auto header = region_writer_header(self);
	assert(header->rows > 0);
	auto offset = (uint32_t*)(self->meta.start + sizeof(Region));
	return (Row*)(self->data.start + offset[header->rows - 1]);
}

static inline Row*
region_writer_last(RegionWriter* self)
{
	if (region_writer_header(self)->rows == 0)
		return NULL;

	return region_writer_max(self);
}

static inline void
region_writer_add_to_iov(RegionWriter* self, Iov* iov)
{
	if (buf_size(&self->compressed) > 0)
	{
		iov_add(iov, self->compressed.start, buf_size(&self->compressed));
	} else
	{
		iov_add(iov, self->meta.start, buf_size(&self->meta));
		iov_add(iov, self->data.start, buf_size(&self->data));
	}
}

static inline uint32_t
region_writer_crc(RegionWriter* self)
{
	uint32_t crc;
	if (buf_size(&self->compressed) > 0)
	{
		crc = crc32(0, self->compressed.start, buf_size(&self->compressed));
	} else
	{
		crc = crc32(0, self->meta.start, buf_size(&self->meta));
		crc = crc32(crc, self->data.start, buf_size(&self->data));
	}
	return crc;
}
