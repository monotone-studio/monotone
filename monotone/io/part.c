
//
// monotone
//
// time-series storage
//

#include <monotone_runtime.h>
#include <monotone_lib.h>
#include <monotone_io.h>

Part*
part_allocate(Comparator* comparator,
              Storage*    storage,
              uint64_t    min,
              uint64_t    max)
{
	auto self = (Part*)mn_malloc(sizeof(Part));
	self->service    = false;
	self->state      = 0;
	self->min        = min;
	self->max        = max;
	self->index      = NULL;
	self->storage    = storage;
	self->comparator = comparator;
	blob_init(&self->mmap, 1 * 1024 * 1024);
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

static inline void
part_free_data(Part* self)
{
	memtable_free(&self->memtable_a);
	memtable_free(&self->memtable_b);
	blob_free(&self->mmap);
	file_close(&self->file);
	buf_free(&self->index_buf);
	buf_init(&self->index_buf);
}

void
part_free(Part* self)
{
	part_free_data(self);
	mn_free(self);
}

static inline void
part_path(char* path, Storage* storage, uint64_t min, bool index)
{
	snprintf(path, PATH_MAX, "%s/%020" PRIu64 "%s", str_of(&storage->path),
	         min, index ? ".index" : "");
}

static inline void
part_path_incomplete(char* path, Storage* storage, uint64_t min, bool index)
{
	snprintf(path, PATH_MAX, "%s/%020" PRIu64 "%s.incomplete", str_of(&storage->path),
	         min, index ? ".index" : "");
}

void
part_open(Part* self, bool check_crc)
{
	// open data file
	char path[PATH_MAX];
	part_path(path, self->storage, self->min, false);
	file_open(&self->file, path);

	// open index file
	part_path(path, self->storage, self->min, true);

	File file;
	file_init(&file);
	guard(guard, file_close, &file);
	file_open(&self->file, path);

	if (file.size < sizeof(Index))
		error("partition: index file '%s' has incorrect size",
		      str_of(&file.path));

	// read and validate index
	file_pread_buf(&file, &self->index_buf, file.size, 0);

	// check index header size
	self->index = (Index*)self->index_buf.start;
	if (file.size != self->index->size)
		error("partition: index file '%s' size mismatch",
		      str_of(&file.path));

	// check crc
	if (check_crc)
	{
		uint32_t crc;
		crc = crc32(0, self->index_buf.start + sizeof(uint32_t),
		            buf_size(&self->index_buf) - sizeof(uint32_t));
		if (crc != self->index->crc)
			error("partition: index file '%s' crc mismatch",
			      str_of(&file.path));
	}

	// check data file size
	if (file.size != self->index->size_total)
		error("partition: data file '%s' size mismatch",
		      str_of(&self->file.path));
}

void
part_create(Part* self, bool sync)
{
	assert(self->mmap.start);

	// create min.max.incomplete file
	char path[PATH_MAX];
	part_path_incomplete(path, self->storage, self->min, false);
	file_create(&self->file, path);
	file_write(&self->file, self->mmap.start, blob_size(&self->mmap));

	// create min.max.index.incomplete file
	part_path_incomplete(path, self->storage, self->min, true);

	File file;
	file_init(&file);
	guard(guard, file_close, &file);
	file_create(&file, path);
	file_write(&file, self->index_buf.start, buf_size(&self->index_buf));

	// sync files
	if (sync)
	{
		file_sync(&self->file);
		file_sync(&file);
	}
}

void
part_delete(Part* self, bool complete)
{
	// delete min.max.index file
	char path[PATH_MAX];
	if (complete)
		part_path(path, self->storage, self->min, true);
	else
		part_path_incomplete(path, self->storage, self->min, true);
	if (fs_exists("%s", path))
		fs_unlink("%s", path);

	// delete min.max file
	if (complete)
		part_path(path, self->storage, self->min, false);
	else
		part_path_incomplete(path, self->storage, self->min, false);
	if (fs_exists("%s", path))
		fs_unlink("%s", path);
}

void
part_rename(Part* self)
{
	// rename to min.max.index file
	char path[PATH_MAX];
	char path_to[PATH_MAX];
	part_path_incomplete(path, self->storage, self->min, true);
	part_path(path_to, self->storage, self->min, true);
	fs_rename(path, "%s", path_to);

	// rename to min.max file
	part_path_incomplete(path, self->storage, self->min, false);
	part_path(path_to, self->storage, self->min, false);
	fs_rename(path, "%s", path_to);
}

void
part_unload(Part* self)
{
	blob_free(&self->mmap);
}
