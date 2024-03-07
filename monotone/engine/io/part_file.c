
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

static void
part_open_file(Part* self, bool read_index)
{
	// open data file and read index

	// <source_path>/<min>
	char path[PATH_MAX];
	id_path(&self->id, self->source, ID, path);
	file_open(&self->file, path);

	if (unlikely(self->file.size < (sizeof(Index) + sizeof(IndexEof))))
		error("partition: file '%s' has incorrect size",
		      str_of(&self->file.path));

	// [data][index][index_eof]
	IndexEof eof;
	file_pread(&self->file, &eof, sizeof(eof),
	            self->file.size - sizeof(IndexEof));

	// check magic
	if (unlikely(eof.magic != INDEX_MAGIC))
		error("partition: file '%s' corrupted", str_of(&self->file.path));

	// validate index offset
	int64_t offset = self->file.size - (sizeof(IndexEof) + eof.size);
	if (offset < 0)
		error("partition: file '%s' size mismatch",
		      str_of(&self->file.path));

	if (! read_index)
	{
		assert(self->index);
		return;
	}

	// read index
	file_pread_buf(&self->file, &self->index_buf, eof.size, offset);

	// set and validate index
	self->index = (Index*)self->index_buf.start;

	if (offset != (int64_t)self->index->size_regions)
		error("partition: file '%s' size mismatch",
		      str_of(&self->file.path));

	// check crc
	if (self->source->crc)
	{
		uint32_t crc;
		crc = crc32(0, self->index_buf.start + sizeof(uint32_t),
		            buf_size(&self->index_buf) - sizeof(uint32_t));
		if (crc != self->index->crc)
			error("partition: file '%s' crc mismatch",
			      str_of(&self->file.path));
	}
}

static void
part_open_file_cloud(Part* self)
{
	// open and read cloud file as index

	// <source_path>/<min>.cloud
	char path[PATH_MAX];
	id_path(&self->id, self->source, ID_CLOUD, path);

	File file;
	file_init(&file);
	file_open(&file, path);
	guard(close, file_close, &file);

	if (unlikely(file.size < (sizeof(Index))))
		error("partition: file '%s' has incorrect size",
		      str_of(&self->file.path));

	// read index
	file_pread_buf(&file, &self->index_buf, file.size, 0);

	// set and validate index
	self->index = (Index*)self->index_buf.start;

	// check crc
	if (self->source->crc)
	{
		uint32_t crc;
		crc = crc32(0, self->index_buf.start + sizeof(uint32_t),
		            buf_size(&self->index_buf) - sizeof(uint32_t));
		if (crc != self->index->crc)
			error("partition: cloud file '%s' crc mismatch",
			      str_of(&file.path));
	}
}

void
part_open(Part* self, int state, bool read_index)
{
	switch (state) {
	case ID:
		part_open_file(self, read_index);
		break;
	case ID_CLOUD:
		part_open_file_cloud(self);
		break;
	default:
		abort();
		break;
	}
}

void
part_create(Part* self, int state)
{
	// <source_path>/<min>.incomplete
	// <source_path>/<min>.cloud.incomplete
	char path[PATH_MAX];
	id_path(&self->id, self->source, state, path);
	switch (state) {
	case ID_INCOMPLETE:
	{
		file_create(&self->file, path);
		break;
	}
	case ID_CLOUD_INCOMPLETE:
	{
		// create, write and sync incomplete cloud file
		File file;
		file_init(&file);
		file_create(&file, path);
		guard(close, file_close, &file);
		file_write_buf(&file, &self->index_buf);
		if (self->source->sync)
			file_sync(&file);
		file_close(&file);
		break;
	}
	default:
		abort();
		break;
	}
}

void
part_delete(Part* self, int state)
{
	// <source_path>/<min>
	// <source_path>/<min>.incomplete
	// <source_path>/<min>.complete
	// <source_path>/<min>.cloud
	// <source_path>/<min>.cloud.incomplete
	char path[PATH_MAX];
	id_path(&self->id, self->source, state, path);
	if (fs_exists("%s", path))
		fs_unlink("%s", path);
}

void
part_rename(Part* self, int from, int to)
{
	// rename file from one state to another
	char path_from[PATH_MAX];
	char path_to[PATH_MAX];
	id_path(&self->id, self->source, from, path_from);
	id_path(&self->id, self->source, to, path_to);
	if (fs_exists("%s", path_from))
		fs_rename(path_from, "%s", path_to);
}
