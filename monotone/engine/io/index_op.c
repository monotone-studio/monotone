
//
// monotone
//
// time-series storage
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
		error("partition: file '%s' corrupted", str_of(&self->path));

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
}

static inline void
compression_push_guard(Compression* self)
{
	compression_mgr_push(global()->compression_mgr, self);
}

void
index_read(File*   self,
           Source* source,
           Index*  index,
           Buf*    index_data,
           bool    copy)
{
	// empty partition file
	if (index->size == 0)
		return;

	// read uncompressed index
	uint64_t offset = self->size - (sizeof(Index) + index->size);
	if (copy || index->compression == COMPRESSION_NONE)
	{
		// read index
		file_pread_buf(self, index_data, index->size, offset);
		return;
	}

	// get compression context
	auto cp = compression_mgr_pop(global()->compression_mgr, index->compression);
	if (cp == NULL)
		error("partition: file '%s' unknown compression id: %d",
		      str_of(&self->path), index->compression);
	guard(guard_cp, compression_push_guard, cp);

	Buf data;
	buf_init(&data);
	guard(guard, buf_free, &data);
	buf_reserve(&data, index->size_origin);

	// read and decompress index
	file_pread_buf(self, &data, index->size, offset);
	compression_decompress(cp, index_data, data.start,
	                       index->size,
	                       index->size_origin);

	// check index data crc
	if (source->crc)
	{
		uint32_t crc = crc32(0, index_data->start, buf_size(index_data));
		if (crc != index->crc_data)
			error("partition: file index data '%s' crc mismatch",
			      str_of(&self->path));
	}
}
