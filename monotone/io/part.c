
//
// monotone
//
// time-series storage
//

#include <monotone_runtime.h>
#include <monotone_lib.h>
#include <monotone_config.h>
#include <monotone_io.h>

static inline void
part_path(char* path, Target* target, uint64_t min)
{
	snprintf(path, PATH_MAX, "%s/%020" PRIu64,
	         str_of(&target->path), min);
}

static inline void
part_path_incomplete(char* path, Target* target, uint64_t min)
{
	snprintf(path, PATH_MAX, "%s/%020" PRIu64 ".incomplete",
	         str_of(&target->path), min);
}

Part*
part_allocate(Comparator* comparator,
              Target*     target,
              uint64_t    min,
              uint64_t    max)
{
	auto self = (Part*)mn_malloc(sizeof(Part));
	self->service    = false;
	self->state      = 0;
	self->min        = min;
	self->max        = max;
	self->index      = NULL;
	self->target     = target;
	self->comparator = comparator;
	file_init(&self->file);
	self->memtable = &self->memtable_a;
	memtable_init(&self->memtable_a, 512, 508, comparator);
	memtable_init(&self->memtable_b, 512, 508, comparator);
	buf_init(&self->index_buf);
	rbtree_init_node(&self->node);
	list_init(&self->link_tier);
	list_init(&self->link);
	return self;
}

void
part_free(Part* self)
{
	memtable_free(&self->memtable_a);
	memtable_free(&self->memtable_b);
	file_close(&self->file);
	buf_free(&self->index_buf);
	mn_free(self);
}

void
part_open(Part* self, bool check_crc)
{
	// open data file and read index
	char path[PATH_MAX];
	part_path(path, self->target, self->min);
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

	// read index
	file_pread_buf(&self->file, &self->index_buf, eof.size, offset);

	// set and validate index
	self->index = (Index*)self->index_buf.start;

	if (offset != (int64_t)self->index->size_total)
		error("partition: file '%s' size mismatch",
		      str_of(&self->file.path));

	// check crc
	if (check_crc)
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
part_create(Part* self)
{
	// create <id>.incomplete file
	char path[PATH_MAX];
	part_path_incomplete(path, self->target, self->min);
	file_create(&self->file, path);

#if 0
	file_write(&self->file, self->mmap.start, blob_size(&self->mmap));
	file_write(&self->file, self->index_buf.start, buf_size(&self->index_buf));
	// sync
	if (sync)
		file_sync(&self->file);
#endif
}

void
part_delete(Part* self, bool complete)
{
	// <id>
	// <id>.incomplete
	char path[PATH_MAX];
	if (complete)
		part_path(path, self->target, self->min);
	else
		part_path_incomplete(path, self->target, self->min);
	if (fs_exists("%s", path))
		fs_unlink("%s", path);
}

void
part_rename(Part* self)
{
	// rename <id>.incomplete to <id>
	char path[PATH_MAX];
	char path_to[PATH_MAX];
	part_path_incomplete(path, self->target, self->min);
	part_path(path_to, self->target, self->min);
	if (fs_exists("%s", path))
		fs_rename(path, "%s", path_to);
}
