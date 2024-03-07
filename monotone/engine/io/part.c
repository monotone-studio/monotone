
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

Part*
part_allocate(Comparator* comparator, Source* source, Id* id)
{
	auto self = (Part*)mn_malloc(sizeof(Part));
	self->id         = *id;
	self->state      = ID_NONE;
	self->time       = 0;
	self->refresh    = false;
	self->cloud      = NULL;
	self->source     = source;
	self->comparator = comparator;
	file_init(&self->file);
	self->memtable = &self->memtable_a;
	memtable_init(&self->memtable_a, 512, 508, comparator);
	memtable_init(&self->memtable_b, 512, 508, comparator);
	index_init(&self->index);
	buf_init(&self->index_data);
	list_init(&self->link);
	return self;
}

void
part_free(Part* self)
{
	memtable_free(&self->memtable_a);
	memtable_free(&self->memtable_b);
	file_close(&self->file);
	buf_free(&self->index_data);
	mn_free(self);
}

void
part_open(Part* self, int state, bool read_index)
{
	switch (state) {
	case ID:
	{
		// open and validate partition file
		index_open(&self->file, self->source, &self->id, state, &self->index);
		if (read_index)
			index_read(&self->file, self->source, &self->index,
			           &self->index_data, false);
		break;
	}
	case ID_CLOUD:
	{
		// open and validate partition cloud file
		File file;
		file_init(&file);
		guard(close, file_close, &file);

		index_open(&file, self->source, &self->id, state, &self->index);
		index_read(&file, self->source, &self->index,
		           &self->index_data, false);
		break;
	}
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
	switch (state) {
	case ID_INCOMPLETE:
	{
		id_path(&self->id, self->source, state, path);
		file_create(&self->file, path);
		break;
	}
	case ID_CLOUD_INCOMPLETE:
	{
		assert(part_has(self, ID));

		// copy existing partition index
		id_path(&self->id, self->source, ID, path);

		File file;
		file_init(&file);
		guard(close, file_close, &file);

		Buf index_data;
		buf_init(&index_data);
		guard(buf_guard, buf_free, &index_data);

		Index index;
		index_open(&file, self->source, &self->id, ID, &index);
		index_read(&file, self->source, &index,
		           &index_data, true);

		file_close(&file);

		// create, write and sync incomplete cloud file
		id_path(&self->id, self->source, state, path);

		file_init(&file);
		file_create(&file, path);
		file_write_buf(&file, &index_data);
		file_write(&file, &index, sizeof(index));
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

void
part_download(Part* self)
{
	assert(part_has(self, ID_CLOUD));
	assert(self->cloud);

	// download partition file locally
	cloud_download(self->cloud, self->source, &self->id);

	// open
	part_open(self, ID, false);
}

void
part_upload(Part* self)
{
	assert(part_has(self, ID));
	assert(self->cloud);

	// in case previous attempt failed without crash
	part_delete(self, ID_CLOUD_INCOMPLETE);

	// create incomplete cloud file (index dump)
	part_create(self, ID_CLOUD_INCOMPLETE);

	// upload partition file to the cloud
	cloud_upload(self->cloud, self->source, &self->id);

	// rename partition cloud file as completed
	part_rename(self, ID_CLOUD_INCOMPLETE, ID_CLOUD);
}

void
part_offload(Part* self, bool from_storage)
{
	// remove from storage
	if (from_storage)
	{
		// remove data file
		file_close(&self->file);
		part_delete(self, ID);
		return;
	}

	// remove from cloud
	assert(self->cloud);

	// remove cloud file first
	part_delete(self, ID_CLOUD);

	// remove from cloud
	cloud_remove(self->cloud, self->source, &self->id);
}
