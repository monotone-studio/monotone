
//
// noire
//
// time-series storage
//

#include <noire_runtime.h>
#include <noire_lib.h>
#include <noire_io.h>

static inline void
part_path(char* path, uint64_t min, uint64_t max)
{
	snprintf(path, PATH_MAX, "%s/%020" PRIu64 ".%020" PRIu64,
	         config_directory(),
	         min, max);
}

static inline void
part_path_incomplete(char* path, uint64_t min, uint64_t max)
{
	snprintf(path, PATH_MAX, "%s/%020" PRIu64 ".%020" PRIu64 ".incomplete",
	         config_directory(),
	         min, max);
}

void
part_open(Part* self, bool check_crc)
{
	// open data file and read index
	char path[PATH_MAX];
	part_path(path, self->min, self->max);
	file_open(&self->file, path);

	if (unlikely(self->file.size < (sizeof(Index) + sizeof(IndexEof))))
		goto corrupted;

	// [data][index][index_eof]
	IndexEof eof;
	file_pread(&self->file, &eof, sizeof(eof),
	            self->file.size - sizeof(IndexEof));

	// check magic
	if (unlikely(eof.magic != INDEX_MAGIC))
		goto corrupted;

	// validate index offset
	int64_t offset = self->file.size - (sizeof(IndexEof) + eof.size);
	if (offset < 0)
		goto corrupted;

	// read index
	file_pread_buf(&self->file, &self->index_buf, eof.size, offset);

	// set and validate index
	self->index = (Index*)self->index_buf.start;

	if (offset != (int64_t)self->index->size_total)
		goto corrupted;

	// check crc
	if (check_crc)
	{
		uint32_t crc;
		crc = crc32(0, self->index_buf.start + sizeof(uint32_t),
		            buf_size(&self->index_buf) - sizeof(uint32_t));
		if (crc != self->index->crc)
			goto corrupted;
	}

	return;

corrupted:
	error("partition: file '%s' is corrupted", self->file.path);
}

void
part_create(Part* self)
{
	char path[PATH_MAX];
	part_path_incomplete(path, self->min, self->max);
	file_create(&self->file, path);
	if (self->mmap.start)
		file_write(&self->file, self->mmap.start, blob_size(&self->mmap));
	file_write(&self->file, self->index_buf.start, buf_size(&self->index_buf));
}

void
part_sync(Part* self)
{
	file_sync(&self->file);
}

void
part_delete(Part* self, bool complete)
{
	char path[PATH_MAX];
	if (complete)
		part_path(path, self->min, self->max);
	else
		part_path_incomplete(path, self->min, self->max);
	fs_unlink("%s", path);
}

void
part_complete(Part* self)
{
	char path[PATH_MAX];
	char path_to[PATH_MAX];
	part_path_incomplete(path, self->min, self->max);
	part_path(path_to, self->min, self->max);
	fs_rename(path, "%s", path_to);
}
