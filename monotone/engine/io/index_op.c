
//
// monotone.
//
// embeddable cloud-native storage for events
// and time-series data.
//
// Copyright (c) 2023-2024 Dmitry Simonenko
// Copyright (c) 2023-2024 Monotone
//
// Distributed under the terms of GNU LGPL v3 or
// Monotone Commercial License.
//

#include <monotone_runtime.h>
#include <monotone_lib.h>
#include <monotone_config.h>
#include <monotone_cloud.h>
#include <monotone_io.h>

void
index_open(File*   self,
           Source* source,
           Id*     id,
           int     type,
           Index*  index)
{
	// open partition or cloud file and read footer

	// <source_path>/<min>
	// <source_path>/<min>.cloud
	char path[PATH_MAX];
	id_path(id, source, type, path);
	file_open(self, path);

	if (unlikely(self->size < sizeof(Index)))
		error("partition: file '%s' has incorrect size",
		      str_of(&self->path));

	// [regions][index_data][index]
	
	// read index
	int64_t offset = self->size - sizeof(Index);
	file_pread(self, index, sizeof(Index), offset);

	// check index magic
	if (unlikely(index->magic != INDEX_MAGIC))
		error("partition: file '%s' is corrupted", str_of(&self->path));

	// check index crc
	uint32_t crc;
	crc = crc32(0, (char*)index + sizeof(uint32_t), sizeof(Index) - sizeof(uint32_t));
	if (crc != index->crc)
		error("partition: file index '%s' crc mismatch",
		      str_of(&self->path));

	// validate file size according to the index and the file type
	bool valid;
	if (type == ID_CLOUD)
		valid = self->size == (index->size + sizeof(Index));
	else
		valid = self->size == (index->size_total);
	if (! valid)
		error("partition: file '%s' size mismatch",
		      str_of(&self->path));

	// check index format version
	if (unlikely(index->version > INDEX_VERSION))
		error("partition: file '%s' index format is not compatible",
		      str_of(&self->path));
}

static inline void
encryption_push_guard(Encryption* self)
{
	encryption_mgr_push(global()->encryption_mgr, self);
}

static inline void
compression_push_guard(Compression* self)
{
	compression_mgr_push(global()->compression_mgr, self);
}

static Buf*
index_decrypt(File*   self,
              Source* source,
              Index*  index,
              Buf*    origin,
              Buf*    buf)
{
	if (index->encryption == ENCRYPTION_NONE)
		return origin;

	// get encryption context
	auto context = encryption_mgr_pop(global()->encryption_mgr, index->encryption);
	if (context == NULL)
		error("partition: file '%s' unknown encryption: %d",
		      str_of(&self->path), index->encryption);
	guard(encryption_push_guard, context);

	// decrypt
	encryption_decrypt(context,
	                   &source->encryption_key,
	                   buf,
	                   origin->start,
	                   buf_size(origin));

	return buf;
}

static Buf*
index_decompress(File*   self,
                 Source* source,
                 Index*  index,
                 Buf*    origin,
                 Buf*    buf)
{
	unused(source);
	if (index->compression == COMPRESSION_NONE)
		return origin;

	// get compression context
	auto context = compression_mgr_pop(global()->compression_mgr, index->compression);
	if (context == NULL)
		error("partition: file '%s' unknown compression id: %d",
		      str_of(&self->path), index->compression);
	guard(compression_push_guard, context);

	// decompress
	compression_decompress(context, buf, origin->start,
	                       buf_size(origin),
	                       index->size_origin);

	return buf;
}

void
index_read(File*   self,
           Source* source,
           Index*  index,
           Buf*    index_data,
           bool    copy)
{
	unused(source);

	// empty partition file
	if (index->size == 0)
		return;

	// read index data
	uint64_t offset = self->size - (sizeof(Index) + index->size);
	file_pread_buf(self, index_data, index->size, offset);

	// check index data crc
	uint32_t crc = crc32(0, index_data->start, buf_size(index_data));
	if (crc != index->crc_data)
		error("partition: file index data '%s' crc mismatch",
		      str_of(&self->path));

	if (copy)
		return;

	auto origin = index_data;

	// decrypt index data
	Buf buf_decrypted;
	buf_init(&buf_decrypted);
	guard(buf_free, &buf_decrypted);
	origin = index_decrypt(self, source, index, origin, &buf_decrypted);

	// decompress index data
	Buf buf_decompressed;
	buf_init(&buf_decompressed);
	guard(buf_free, &buf_decompressed);
	origin = index_decompress(self, source, index, origin, &buf_decompressed);

	// return
	if (origin != index_data)
	{
		buf_reset(index_data);
		buf_write(index_data, origin->start, buf_size(origin));
	}
}
