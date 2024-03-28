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
	Buf          data;
	Buf          compressed;
	Compression* compression;
	int          compression_level;
	Buf          encrypted;
	Encryption*  encryption;
	Str*         encryption_key;
	bool         crc;
	Index        index;
};

static inline void
index_writer_init(IndexWriter* self)
{
	self->active            = false;
	self->compression       = NULL;
	self->compression_level = 0;
	self->encryption        = NULL;
	self->encryption_key    = NULL;
	self->crc               = false;
	buf_init(&self->data);
	buf_init(&self->compressed);
	buf_init(&self->encrypted);
	memset(&self->index, 0, sizeof(self->index));
}

static inline void
index_writer_free(IndexWriter* self)
{
	buf_free(&self->data);
	buf_free(&self->compressed);
	buf_free(&self->encrypted);
}

static inline void
index_writer_reset(IndexWriter* self)
{
	self->active            = false;
	self->compression       = NULL;
	self->compression_level = 0;
	self->encryption        = NULL;
	self->encryption_key    = NULL;
	self->crc               = false;
	buf_reset(&self->data);
	buf_reset(&self->compressed);
	buf_reset(&self->encrypted);
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
                   Encryption*  encryption,
                   Str*         encryption_key,
                   bool         crc)
{
	self->compression       = compression;
	self->compression_level = compression_level;
	self->encryption        = encryption;
	self->encryption_key    = encryption_key;
	self->crc               = crc;
	self->active            = true;
}

static inline void
index_writer_stop(IndexWriter* self,
                  Id*          id,
                  uint32_t     refreshes,
                  uint64_t     time,
                  uint64_t     lsn)
{
	auto index = &self->index;

	// compress index data (without index header)
	uint32_t size_origin = index->size;
	uint32_t size = size_origin;
	int      compression_id;
	if (self->compression)
	{
		Buf* argv[] = { &self->data };
		compression_compress(self->compression, &self->compressed,
		                     self->compression_level,
		                     1, argv);
		size = buf_size(&self->compressed);
		compression_id = self->compression->iface->id;
	} else {
		compression_id = COMPRESSION_NONE;
	}

	// encrypt index data
	int encryption_id;
	if (self->encryption)
	{
		Buf* argv[1];
		if (self->compression)
			argv[0] = &self->compressed;
		else
			argv[0] = &self->data;
		encryption_encrypt(self->encryption, global()->random,
		                   self->encryption_key,
		                   &self->encrypted,
		                   1, argv);
		size = buf_size(&self->encrypted);
		encryption_id = self->encryption->iface->id;
	} else
	{
		encryption_id = ENCRYPTION_NONE;
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
	index->refreshes         = refreshes;
	index->time              = time;
	index->lsn               = lsn;
	index->compression       = compression_id;
	index->encryption        = encryption_id;

	// calculate index data crc
	uint32_t crc = 0;
	if (self->encryption)
		crc = crc32(crc, self->encrypted.start, buf_size(&self->encrypted));
	else
	if (self->compression)
		crc = crc32(crc, self->compressed.start, buf_size(&self->compressed));
	else
		crc = crc32(crc, self->data.start, buf_size(&self->data));
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

	uint32_t size;
	if (self->encryption)
		size = buf_size(&region_writer->encrypted);
	else
	if (self->compression)
		size = buf_size(&region_writer->compressed);
	else
		size = region->size;

	uint32_t crc = 0;
	if (self->crc)
		crc = region_writer_crc(region_writer);

	// prepare meta region reference
	buf_reserve(&self->data, sizeof(IndexRegion));
	auto ref = (IndexRegion*)self->data.position;
	ref->offset      = region_offset;
	ref->crc         = crc;
	ref->min         = region_writer_min(region_writer)->id;
	ref->max         = region_writer_max(region_writer)->id;
	ref->size        = size;
	ref->size_origin = region->size;
	ref->events      = region->events;
	memset(ref->reserved, 0, sizeof(ref->reserved));
	buf_advance(&self->data, sizeof(IndexRegion));

	// update header
	auto index = &self->index;
	index->regions++;
	index->events += region->events;
	index->size += sizeof(IndexRegion);
	index->size_regions += size;
	index->size_regions_origin += region->size;
}

static inline void
index_writer_copy(IndexWriter* self, Index* index, Buf* buf)
{
	*index = self->index;
	buf_write(buf, self->data.start, buf_size(&self->data));
}

static inline void
index_writer_add_to_iov(IndexWriter* self, Iov* iov)
{
	if (self->encryption)
		iov_add(iov, self->encrypted.start, buf_size(&self->encrypted));
	else
	if (self->compression)
		iov_add(iov, self->compressed.start, buf_size(&self->compressed));
	else
		iov_add(iov, self->data.start, buf_size(&self->data));
	iov_add(iov, &self->index, sizeof(self->index));
}
