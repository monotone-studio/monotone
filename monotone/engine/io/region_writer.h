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
	Buf          encrypted;
	Compression* compression;
	int          compression_level;
	Encryption*  encryption;
	Str*         encryption_key;
};

static inline void
region_writer_init(RegionWriter* self)
{
	self->compression       = NULL;
	self->compression_level = 0;
	self->encryption        = NULL;
	self->encryption_key    = NULL;
	buf_init(&self->meta);
	buf_init(&self->data);
	buf_init(&self->compressed);
	buf_init(&self->encrypted);
}

static inline void
region_writer_free(RegionWriter* self)
{
	buf_free(&self->meta);
	buf_free(&self->data);
	buf_free(&self->compressed);
	buf_free(&self->encrypted);
}

static inline void
region_writer_reset(RegionWriter* self)
{
	self->compression       = NULL;
	self->compression_level = 0;
	self->encryption        = NULL;
	self->encryption_key    = NULL;
	buf_reset(&self->meta);
	buf_reset(&self->data);
	buf_reset(&self->compressed);
	buf_reset(&self->encrypted);
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
region_writer_start(RegionWriter* self,
                    Compression*  compression,
                    int           compression_level,
                    Encryption*   encryption,
                    Str*          encryption_key)
{
	self->compression       = compression;
	self->compression_level = compression_level;
	self->encryption        = encryption;
	self->encryption_key    = encryption_key;

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
	int  argc;
	Buf* argv[2];
	if (self->compression)
	{
		argc    = 2;
		argv[0] = &self->meta;
		argv[1] = &self->data;
		compression_compress(self->compression, &self->compressed,
		                     self->compression_level,
		                     argc, argv);
	}

	// encrypt region using compressed or raw data
	if (self->encryption)
	{
		if (self->compression)
		{
			argc    = 1;
			argv[0] = &self->compressed;
		} else
		{
			argc    = 2;
			argv[0] = &self->meta;
			argv[1] = &self->data;
		}
		encryption_encrypt(self->encryption, global()->random,
		                   self->encryption_key,
		                   &self->encrypted,
		                   argc, argv);
	}
}

static inline void
region_writer_add(RegionWriter* self, Event* event)
{
	// write data offset
	uint32_t offset = buf_size(&self->data);
	buf_write(&self->meta, &offset, sizeof(offset));

	// copy data
	uint32_t size = event_size(event);
	buf_write(&self->data, event, size);

	// update header
	auto header = region_writer_header(self);
	header->events++;
}

static inline Event*
region_writer_min(RegionWriter* self)
{
	return (Event*)self->data.start;
}

static inline Event*
region_writer_max(RegionWriter* self)
{
	auto header = region_writer_header(self);
	assert(header->events > 0);
	auto offset = (uint32_t*)(self->meta.start + sizeof(Region));
	return (Event*)(self->data.start + offset[header->events - 1]);
}

static inline Event*
region_writer_last(RegionWriter* self)
{
	if (region_writer_header(self)->events == 0)
		return NULL;

	return region_writer_max(self);
}

static inline void
region_writer_add_to_iov(RegionWriter* self, Iov* iov)
{
	if (self->encryption)
	{
		iov_add(iov, self->encrypted.start, buf_size(&self->encrypted));
	} else
	if (self->compression)
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
	uint32_t crc = 0;
	if (self->encryption)
	{
		crc = crc32(crc, self->encrypted.start, buf_size(&self->encrypted));
	} else
	if (self->compression)
	{
		crc = crc32(crc, self->compressed.start, buf_size(&self->compressed));
	} else
	{
		crc = crc32(crc, self->meta.start, buf_size(&self->meta));
		crc = crc32(crc, self->data.start, buf_size(&self->data));
	}
	return crc;
}
