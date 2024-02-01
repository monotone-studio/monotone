
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
part_path(char* path, Source* source, PartId* id)
{
	// <source_path>/<min>.<id>
	source_pathfmt(source, path, PATH_MAX, "%020" PRIu64 ".%020" PRIu64,
	               id->min, id->id);
}

static inline void
part_path_incomplete(char* path, Source* source, PartId* id)
{
	// <source_path>/<min>.<id>.<id_parent>
	source_pathfmt(source, path, PATH_MAX, "%020" PRIu64 ".%020" PRIu64 ".%020" PRIu64,
	               id->min, id->id, id->id_parent);
}

Part*
part_allocate(Comparator* comparator, Source* source, PartId* id)
{
	auto self = (Part*)mn_malloc(sizeof(Part));
	self->refresh    = false;
	self->id         = *id;
	self->index      = NULL;
	self->source     = source;
	self->comparator = comparator;
	file_init(&self->file);
	self->memtable = &self->memtable_a;
	memtable_init(&self->memtable_a, 512, 508, comparator);
	memtable_init(&self->memtable_b, 512, 508, comparator);
	buf_init(&self->index_buf);
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
part_open(Part* self)
{
	// open data file and read index

	// <source_path>/<min>.<id>
	char path[PATH_MAX];
	part_path(path, self->source, &self->id);
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
	buf_reset(&self->index_buf);
	file_pread_buf(&self->file, &self->index_buf, eof.size, offset);

	// set and validate index
	self->index = (Index*)self->index_buf.start;

	if (offset != (int64_t)self->index->size_total)
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
part_create(Part* self)
{
	// <source_path>/<min>.<id>.<id_parent>
	char path[PATH_MAX];
	part_path_incomplete(path, self->source, &self->id);
	file_create(&self->file, path);
}

void
part_delete(Part* self, bool complete)
{
	// <source_path>/<min>.<psn>
	// <source_path>/<min>.<psn>.<id_parent>
	char path[PATH_MAX];
	if (complete)
		part_path(path, self->source, &self->id);
	else
		part_path_incomplete(path, self->source, &self->id);
	if (fs_exists("%s", path))
		fs_unlink("%s", path);
}

void
part_rename(Part* self)
{
	char path[PATH_MAX];
	char path_to[PATH_MAX];
	part_path_incomplete(path, self->source, &self->id);
	part_path(path_to, self->source, &self->id);
	if (fs_exists("%s", path))
		fs_rename(path, "%s", path_to);
}

Part*
part_download(Cloud*      cloud,
              Comparator* comparator,
              Source*     source,
              PartId*     id)
{
	Part* part = NULL;
	Exception e;
	if (try(&e))
	{
		// download partition file locally
		cloud_download(cloud, id->min, id->id);

		// open
		part = part_allocate(comparator, source, id);
		part_open(part);

		// todo: remove index file
	}
	if (catch(&e))
	{
		if (part)
			part_free(part);
		rethrow();
	}
	return part;
}
