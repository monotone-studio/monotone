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
	Encryption*  encryption;
	Buf          buf;
	Buf          buf_decrypted;
	Buf          buf_decompressed;
};

static inline void
reader_init(Reader* self)
{
	self->part        = NULL;
	self->compression = NULL;
	self->encryption  = NULL;
	buf_init(&self->buf);
	buf_init(&self->buf_decrypted);
	buf_init(&self->buf_decompressed);
}

static inline void
reader_reset(Reader* self)
{
	if (self->compression)
	{
		compression_mgr_push(global()->compression_mgr, self->compression);
		self->compression = NULL;
	}
	if (self->encryption)
	{
		encryption_mgr_push(global()->encryption_mgr, self->encryption);
		self->encryption = NULL;
	}
	buf_reset(&self->buf);
	buf_reset(&self->buf_decrypted);
	buf_reset(&self->buf_decompressed);
}

static inline void
reader_free(Reader* self)
{
	buf_free(&self->buf);
	buf_free(&self->buf_decrypted);
	buf_free(&self->buf_decompressed);
}

static inline void
reader_open(Reader* self, Part* part)
{
	self->part = part;

	// set compression context using partition index
	int compression_id = part->index.compression;
	if (compression_id != COMPRESSION_NONE)
		self->compression =
			compression_mgr_pop(global()->compression_mgr, compression_id);

	// set encryption context
	int encryption_id = part->index.encryption;
	if (encryption_id != ENCRYPTION_NONE)
		self->encryption =
			encryption_mgr_pop(global()->encryption_mgr, encryption_id);
}

hot static inline Region*
reader_execute(Reader* self, IndexRegion* index_region)
{
	auto part = self->part;

	buf_reset(&self->buf);
	buf_reset(&self->buf_decrypted);
	buf_reset(&self->buf_decompressed);

	if (part_has(part, ID))
	{
		// read region data from file
		file_pread_buf(&part->file, &self->buf, index_region->size,
		               index_region->offset);
	} else
	if (part_has(part, ID_CLOUD))
	{
		// read region data from cloud
		cloud_read(part->cloud, part->source, &part->id,
		           &self->buf,
		           index_region->size,
		           index_region->offset);
	} else {
		abort();
	}
	Buf* origin = &self->buf;

	// decrypt region
	if (self->encryption)
	{
		encryption_decrypt(self->encryption,
		                   &part->source->encryption_key,
		                   &self->buf_decrypted,
		                   origin->start,
		                   buf_size(origin));
		origin = &self->buf_decrypted;
	}

	// decompress region
	if (self->compression)
	{
		compression_decompress(self->compression,
		                       &self->buf_decompressed,
		                       origin->start,
		                       buf_size(origin),
		                       index_region->size_origin);
		origin = &self->buf_decompressed;
	}

	// consistency check
	auto region = (Region*)origin->start;
	if (region->size   != index_region->size_origin ||
	    region->events != index_region->events)
		error("read: region meta data mismatch");

	return region;
}
