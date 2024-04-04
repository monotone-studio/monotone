
//
// monotone
//
// time-series storage
//

#include <monotone_runtime.h>
#include <monotone_lib.h>
#include <monotone_config.h>
#include <monotone_cloud.h>

static inline void
mock_path(Source* source, Id* id, char* path)
{
	// <source_path>/<min>
	source_pathfmt(source, path, PATH_MAX, "mock/%020" PRIu64, id->min);
}

static void
mock_free(Cloud* self)
{
	if (self->config)
		cloud_config_free(self->config);
	mn_free(self);
}

static Cloud*
mock_create(CloudIf* iface, CloudConfig* config)
{
	auto self = (Cloud*)mn_malloc(sizeof(Cloud));
	self->refs   = 0;
	self->iface  = iface;
	self->config = NULL;
	list_init(&self->link);
	guard_as(self_guard, mock_free, self);
	self->config = cloud_config_copy(config);
	return unguard(&self_guard);
}

static void
mock_attach(Cloud* self, Source* source)
{
	unused(self);
	unused(source);
}

static void
mock_detach(Cloud* self, Source* source)
{
	unused(self);
	unused(source);
}

static inline void
mock_mkdir(Source* source)
{
	// create cloud mock directory, if not exists
	char path[PATH_MAX];
	source_pathfmt(source, path, sizeof(path), "mock");
	if (! fs_exists("%s", path))
	{
		log("mock: new directory '%s'", path);
		fs_mkdir(0755, "%s", path);
	}
}

static void
mock_download(Cloud* self, Source* source, Id* id)
{
	unused(self);

	// create cloud mock directory, if not exists
	mock_mkdir(source);

	// copy file from mock directory to the storage
	char path_from[PATH_MAX];
	char path_to[PATH_MAX];
	mock_path(source, id, path_from);
	id_path(id, source, ID_INCOMPLETE, path_to);

	// read mock file
	Buf buf;
	buf_init(&buf);
	guard(buf_free, &buf);
	file_import(&buf, "%s", path_from);

	// in case previous attempt failed without crash
	if (fs_exists("%s", path_to))
		fs_unlink("%s", path_to);

	// create incomplete file
	File file;
	file_init(&file);
	guard(file_close, &file);
	file_create(&file, path_to);
	file_write_buf(&file, &buf);

	// crash case 4
	error_injection(error_download);

	// rename
	id_path(id, source, ID_INCOMPLETE, path_from);
	id_path(id, source, ID, path_to);
	fs_rename(path_from, "%s", path_to);
}

static void
mock_upload(Cloud* self, Source* source, Id* id)
{
	unused(self);

	// create cloud mock directory, if not exists
	mock_mkdir(source);

	// copy file from storage to the mock directory
	char path_from[PATH_MAX];
	char path_to[PATH_MAX];
	id_path(id, source, ID, path_from);
	mock_path(source, id, path_to);

	// read storage file
	Buf buf;
	buf_init(&buf);
	guard(buf_free, &buf);
	file_import(&buf, "%s", path_from);

	// crash case 5
	error_injection(error_upload);

	// create mock file
	File file;
	file_init(&file);
	guard(file_close, &file);
	file_create(&file, path_to);
	file_write_buf(&file, &buf);
}

static void
mock_remove(Cloud* self, Source* source, Id* id)
{
	unused(self);

	// remove file from mock directory
	char path[PATH_MAX];
	mock_path(source, id, path);
	if (fs_exists("%s", path))
		fs_unlink("%s", path);
}

static void
mock_read(Cloud* self, Source* source, Id* id, Buf* buf, uint32_t size,
          uint64_t offset)
{
	unused(self);

	char path[PATH_MAX];
	mock_path(source, id, path);
	buf_reserve(buf, size);

	// open and read mock file
	File file;
	file_init(&file);
	guard(file_close, &file);
	file_open(&file, path);
	file_pread_buf(&file, buf, size, offset);
}

CloudIf cloud_mock =
{
	.create   = mock_create,
	.free     = mock_free,
	.attach   = mock_attach,
	.detach   = mock_detach,
	.download = mock_download,
	.upload   = mock_upload,
	.remove   = mock_remove,
	.read     = mock_read
};
