#pragma once

//
// monotone
//
// time-series storage
//

typedef struct IndexWriter IndexWriter;

struct IndexWriter
{
	bool     crc;
	int      compression_id;
	Buf      meta;
	Buf      data;
	IndexEof eof;
};

static inline void
index_writer_init(IndexWriter* self)
{
	self->crc            = false;
	self->compression_id = COMPRESSION_NONE;
	buf_init(&self->meta);
	buf_init(&self->data);
}

static inline void
index_writer_free(IndexWriter* self)
{
	buf_free(&self->meta);
	buf_free(&self->data);
}

static inline void
index_writer_reset(IndexWriter* self)
{
	self->crc            = false;
	self->compression_id = COMPRESSION_NONE;
	buf_reset(&self->meta);
	buf_reset(&self->data);
}

static inline Index*
index_writer_header(IndexWriter* self)
{
	assert(self->meta.start != NULL);
	return (Index*)self->meta.start;
}

static inline bool
index_writer_started(IndexWriter* self)
{
	return buf_size(&self->meta) > 0;
}

static inline void
index_writer_start(IndexWriter* self, int compression_id, bool crc)
{
	self->crc            = crc;
	self->compression_id = compression_id;

	// index header
	buf_reserve(&self->meta, sizeof(Index));
	auto header = index_writer_header(self);
	memset(header, 0, sizeof(Index));
	header->size = sizeof(Index);
	buf_advance(&self->meta, sizeof(Index));
}

static inline void
index_writer_stop(IndexWriter* self, Id* id, uint64_t lsn)
{
	// prepare header
	auto header = index_writer_header(self);
	header->compression = self->compression_id;
	header->crc         = 0;
	header->id          = *id;
	header->lsn         = lsn;

	// prepare eof marker
	auto eof = &self->eof;
	eof->magic = INDEX_MAGIC;
	eof->size  = buf_size(&self->meta) + buf_size(&self->data);

	// crc
	if (self->crc)
	{
		uint32_t crc = 0;
		crc = crc32(crc, self->meta.start + sizeof(uint32_t),
		            buf_size(&self->meta) - sizeof(uint32_t));
		crc = crc32(crc, self->data.start, buf_size(&self->data));
		header->crc  = crc;
	}
}

static inline void
index_writer_add(IndexWriter*  self,
                 RegionWriter* region_writer,
                 uint64_t      region_offset)
{
	auto region = region_writer_header(region_writer);
	assert(region->rows > 0);

	// write region offset
	uint32_t offset = buf_size(&self->data);
	buf_write(&self->meta, &offset, sizeof(offset));

	// prepare meta region reference
	buf_reserve(&self->data, sizeof(IndexRegion));

	uint32_t size;
	if (self->compression_id == COMPRESSION_NONE)
		size = region->size;
	else
		size = buf_size(&region_writer->compressed);

	uint32_t crc = 0;
	if (self->crc)
		crc = region_writer_crc(region_writer);
	auto ref = (IndexRegion*)self->data.position;
	ref->offset       = region_offset;
	ref->size_origin  = region->size;
	ref->size         = size;
	ref->size_key_min = 0;
	ref->size_key_max = 0;
	ref->rows         = region->rows;
	ref->crc          = crc;
	memset(ref->reserved, 0, sizeof(ref->reserved));
	buf_advance(&self->data, sizeof(IndexRegion));

	// copy min/max keys
	auto min = region_writer_min(region_writer);
	buf_write(&self->data, min, row_size(min));

	auto max = region_writer_max(region_writer);
	buf_write(&self->data, max, row_size(max));

	ref = (IndexRegion*)(self->data.start + offset);
	ref->size_key_min = row_size(min);
	ref->size_key_max = row_size(max);

	// update header
	auto header = index_writer_header(self);
	header->regions++;
	header->rows += region->rows;
	header->size += sizeof(uint32_t) + sizeof(IndexRegion) +
	                row_size(min) +
	                row_size(max);
	header->size_regions += size;
	header->size_regions_origin += region->size;
}

static inline void
index_writer_copy(IndexWriter* self, Buf* buf)
{
	buf_write(buf, self->meta.start, buf_size(&self->meta));
	buf_write(buf, self->data.start, buf_size(&self->data));
	buf_write(buf, &self->eof,       sizeof(self->eof));
}

static inline void
index_writer_add_to_iov(IndexWriter* self, Iov* iov)
{
	iov_add(iov, self->meta.start, buf_size(&self->meta));
	iov_add(iov, self->data.start, buf_size(&self->data));
	iov_add(iov, &self->eof,       sizeof(self->eof));
}
