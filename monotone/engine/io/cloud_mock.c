
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
mock_path(Id* self, Source* source, char* path)
{
	// <source_path>/<min>
	source_pathfmt(source, path, PATH_MAX, "mock/%020" PRIu64, self->min);
}

static Cloud*
mock_create(CloudIf* iface, Source* source)
{
	auto cloud = (Cloud*)mn_malloc(sizeof(Cloud));
	cloud->iface  = iface;
	cloud->source = source;

	// create cloud mock directory, if not exists
	char path[PATH_MAX];
	source_pathfmt(source, path, sizeof(path), "mock");
	if (! fs_exists("%s", path))
	{
		log("mock: new directory '%s'", path);
		fs_mkdir(0755, "%s", path);
	}

	return cloud;
}

static void
mock_free(Cloud* self)
{
	mn_free(self);
}

static void
mock_download(Cloud* self, Id* id)
{
	// copy file from mock directory to storage
	char path_from[PATH_MAX];
	char path_to[PATH_MAX];
	mock_path(id, self->source, path_from);
	id_path_incomplete(id, self->source, path_to);

	// read mock file
	Buf buf;
	buf_init(&buf);
	guard(guard, buf_free, &buf);
	file_import(&buf, "%s", path_from);

	// create incomplete file
	File file;
	file_init(&file);
	guard(guard_file, file_close, &file);
	file_create(&file, path_to);
	file_write_buf(&file, &buf);
	file_close(&file);

	// rename as completed
	id_path_incomplete(id, self->source, path_from);
	id_path(id, self->source, path_to);
	fs_rename(path_from, "%s", path_to);
}

static void
mock_upload(Cloud* self, Id* id)
{
	// copy file from storage to mock directory
	char path_from[PATH_MAX];
	char path_to[PATH_MAX];
	id_path(id, self->source, path_from);
	mock_path(id, self->source, path_to);

	// read storage file
	Buf buf;
	buf_init(&buf);
	guard(guard, buf_free, &buf);
	file_import(&buf, "%s", path_from);

	// create mock file
	File file;
	file_init(&file);
	guard(guard_file, file_close, &file);
	file_create(&file, path_to);
	file_write_buf(&file, &buf);
}

static void
mock_remove(Cloud* self, Id* id)
{
	// remove file from mock directory
	char path[PATH_MAX];
	mock_path(id, self->source, path);
	if (fs_exists("%s", path))
		fs_unlink("%s", path);
}

static void
mock_read(Cloud* self, Id* id, Buf* buf, uint32_t size, uint64_t offset)
{
	char path[PATH_MAX];
	mock_path(id, self->source, path);
	buf_reserve(buf, size);

	// open and read mock file
	File file;
	file_init(&file);
	guard(guard_file, file_close, &file);
	file_open(&file, path);
	file_pread_buf(&file, buf, size, offset);
	file_close(&file);
}

void
cloud_mock_init(CloudMock* self)
{
	Str name;
	str_set_cstr(&name, "mock");
	cloud_iface_init(&self->iface, &name);

	auto iface = &self->iface;
	iface->create   = mock_create;
	iface->free     = mock_free;
	iface->download = mock_download;
	iface->upload   = mock_upload;
	iface->remove   = mock_remove;
	iface->read     = mock_read;
}
