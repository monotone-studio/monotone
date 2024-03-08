#pragma once

//
// monotone
//
// time-series storage
//

typedef struct IndexWriter IndexWriter;

struct IndexWriter
{
	bool         active;
	Buf          meta;
	Buf          data;
	Buf          compressed;
	Compression* compression;
	int          compression_level;
	bool         crc;
	Index        index;
};

static inline void
index_writer_init(IndexWriter* self)
{
	self->active            = false;
	self->compression       = NULL;
	self->compression_level = 0;
	self->crc               = false;
	buf_init(&self->meta);
	buf_init(&self->data);
	buf_init(&self->compressed);
	memset(&self->index, 0, sizeof(self->index));
}

static inline void
index_writer_free(IndexWriter* self)
{
	buf_free(&self->meta);
	buf_free(&self->data);
	buf_free(&self->compressed);
}

static inline void
index_writer_reset(IndexWriter* self)
{
	self->active            = false;
	self->compression       = NULL;
	self->compression_level = 0;
	self->crc               = false;
	buf_reset(&self->meta);
	buf_reset(&self->data);
	buf_reset(&self->compressed);
	memset(&self->index, 0, sizeof(self->index));
}

static inline bool
index_writer_started(IndexWriter* self)
{
	return self->active;
}

static inline void
index_writer_start(IndexWriter* self,
                   Compression* compression,
                   int          compression_level,
                   bool         crc)
{
	self->compression       = compression;
	self->compression_level = compression_level;
	self->crc               = crc;
	self->active            = true;
}

static inline void
index_writer_stop(IndexWriter* self, Id* id, uint64_t time, uint64_t lsn)
{
	auto index = &self->index;

	// compress index (without index)
	uint32_t size_origin = index->size;
	uint32_t size = size_origin;
	int      compression_id;
	if (self->compression)
	{
		compression_compress(self->compression, &self->compressed,
		                     self->compression_level,
		                     &self->meta, &self->data);
		size = buf_size(&self->compressed);
		compression_id = self->compression->iface->id;
	} else {
		compression_id = COMPRESSION_NONE;
	}

	// prepare index
	index->crc               = 0;
	index->crc_data          = 0;
	index->magic             = INDEX_MAGIC;
	index->id                = *id;
	index->size              = size;
	index->size_origin       = size_origin;
	index->size_total        =
		index->size_regions + index->size + sizeof(Index);
	index->size_total_origin =
		index->size_regions_origin + index->size_origin + sizeof(Index);
	index->time              = time;
	index->lsn               = lsn;
	index->compression       = compression_id;

	// calculate index data crc
	uint32_t crc = 0;
	if (self->compression)
	{
		crc = crc32(crc, self->compressed.start, buf_size(&self->compressed));
	} else
	{
		crc = crc32(crc, self->meta.start, buf_size(&self->meta));
		crc = crc32(crc, self->data.start, buf_size(&self->data));
	}
	index->crc_data = crc;

	// calculate index crc
	index->crc = crc32(0, index, sizeof(Index));
}

static inline void
index_writer_add(IndexWriter*  self,
                 RegionWriter* region_writer,
                 uint64_t      region_offset)
{
	auto region = region_writer_header(region_writer);
	assert(region->events > 0);

	// write region offset
	uint32_t offset = buf_size(&self->data);
	buf_write(&self->meta, &offset, sizeof(offset));

	// prepare meta region reference
	buf_reserve(&self->data, sizeof(IndexRegion));

	uint32_t size;
	if (self->compression)
		size = buf_size(&region_writer->compressed);
	else
		size = region->size;

	uint32_t crc = 0;
	if (self->crc)
		crc = region_writer_crc(region_writer);
	auto ref = (IndexRegion*)self->data.position;
	ref->offset       = region_offset;
	ref->size_origin  = region->size;
	ref->size         = size;
	ref->size_key_min = 0;
	ref->size_key_max = 0;
	ref->events       = region->events;
	ref->crc          = crc;
	memset(ref->reserved, 0, sizeof(ref->reserved));
	buf_advance(&self->data, sizeof(IndexRegion));

	// copy min/max keys
	auto min = region_writer_min(region_writer);
	buf_write(&self->data, min, event_size(min));

	auto max = region_writer_max(region_writer);
	buf_write(&self->data, max, event_size(max));

	ref = (IndexRegion*)(self->data.start + offset);
	ref->size_key_min = event_size(min);
	ref->size_key_max = event_size(max);

	// update header
	auto index = &self->index;
	index->regions++;
	index->events += region->events;
	index->size += sizeof(uint32_t) +
	               sizeof(IndexRegion) + event_size(min) +
	               event_size(max);
	index->size_regions += size;
	index->size_regions_origin += region->size;
}

static inline void
index_writer_copy(IndexWriter* self, Index* index, Buf* buf)
{
	*index = self->index;
	buf_write(buf, self->meta.start, buf_size(&self->meta));
	buf_write(buf, self->data.start, buf_size(&self->data));
}

static inline void
index_writer_add_to_iov(IndexWriter* self, Iov* iov)
{
	if (self->compression)
	{
		iov_add(iov, self->compressed.start, buf_size(&self->compressed));
	} else
	{
		iov_add(iov, self->meta.start, buf_size(&self->meta));
		iov_add(iov, self->data.start, buf_size(&self->data));
	}
	iov_add(iov, &self->index, sizeof(self->index));
}
