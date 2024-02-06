
//
// monotone
//
// time-series storage
//

#include <monotone_runtime.h>
#include <monotone_lib.h>
#include <monotone_config.h>
#include <monotone_io.h>

void
part_file_open(Part* self, bool read_index)
{
	// open data file and read index

	// <source_path>/<min>.<id>
	char path[PATH_MAX];
	id_path(&self->id, self->source, path);
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

void
part_file_create(Part* self)
{
	// <source_path>/<min>.<id>.<id_parent>
	char path[PATH_MAX];
	id_path_incomplete(&self->id, self->source, path);
	file_create(&self->file, path);
}

void
part_file_delete(Part* self, bool complete)
{
	// <source_path>/<min>.<psn>
	// <source_path>/<min>.<psn>.<id_parent>
	char path[PATH_MAX];
	if (complete)
		id_path(&self->id, self->source, path);
	else
		id_path_incomplete(&self->id, self->source, path);
	if (fs_exists("%s", path))
		fs_unlink("%s", path);
}

void
part_file_complete(Part* self)
{
	// rename file from incomplete to complete
	char path[PATH_MAX];
	char path_to[PATH_MAX];
	id_path_incomplete(&self->id, self->source, path);
	id_path(&self->id, self->source, path_to);
	if (fs_exists("%s", path))
		fs_rename(path, "%s", path_to);
}

void
part_file_cloud_open(Part* self)
{
	// open and read cloud file as index

	// <source_path>/<min>.<id>.cloud
	char path[PATH_MAX];
	id_path_cloud(&self->id, self->source, path);

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
part_file_cloud_create(Part* self)
{
	// <source_path>/<min>.<id>.cloud.incomplete
	char path[PATH_MAX];
	id_path_cloud_incomplete(&self->id, self->source, path);

	// create, write and sync incomplete cloud file
	File file;
	file_init(&file);
	file_create(&file, path);
	guard(close, file_close, &file);
	file_write_buf(&file, &self->index_buf);
	file_sync(&file);
	file_close(&file);
}

void
part_file_cloud_delete(Part* self, bool complete)
{
	// <source_path>/<min>.<psn>.cloud
	// <source_path>/<min>.<psn>.cloud.incomplete
	char path[PATH_MAX];
	if (complete)
		id_path_cloud(&self->id, self->source, path);
	else
		id_path_cloud_incomplete(&self->id, self->source, path);
	if (fs_exists("%s", path))
		fs_unlink("%s", path);
}

void
part_file_cloud_complete(Part* self)
{
	// rename file from incomplete to complete
	char path[PATH_MAX];
	char path_to[PATH_MAX];
	id_path_cloud_incomplete(&self->id, self->source, path);
	id_path_cloud(&self->id, self->source, path_to);
	if (fs_exists("%s", path))
		fs_rename(path, "%s", path_to);
}
