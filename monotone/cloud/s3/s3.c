
//
// monotone
//
// time-series storage
//

#include <monotone_runtime.h>
#include <monotone_lib.h>
#include <monotone_config.h>
#include <monotone_cloud.h>
#include <monotone_s3.h>

#include <curl/curl.h>

static void
s3_free(Cloud* self)
{
	auto s3 = s3_of(self);
	list_foreach_safe(&s3->cache.list)
	{
		auto io = list_at(S3Io, link);
		s3_io_free(io);
	}
	if (self->config)
		cloud_config_free(self->config);
	mn_free(self);
}

static void
s3_validate_config(CloudConfig* config)
{
	if (str_empty(&config->login))
		error("s3: access_key is not defined");

	if (str_empty(&config->password))
		error("s3: secret_key is not defined");

	if (str_empty(&config->url))
		error("s3: url is not defined");
}

static Cloud*
s3_create(CloudIf* iface, CloudConfig* config)
{
	// validate config options
	s3_validate_config(config);

	auto self = (S3*)mn_malloc(sizeof(S3));
	cache_init(&self->cache);

	auto cloud = &self->cloud;
	cloud->refs   = 0;
	cloud->iface  = iface;
	cloud->config = NULL;
	list_init(&cloud->link);

	guard(self_guard, s3_free, self);
	cloud->config = cloud_config_copy(config);

	unguard(&self_guard);
	return cloud;
}

static inline S3Io*
s3_get_io(S3* self)
{
	auto link = cache_pop(&self->cache);
	if (likely(link))
		return container_of(link, S3Io, link);
	return s3_io_allocate(&self->cloud);
}

static void
s3_attach(Cloud* self, Source* source)
{
	auto s3 = s3_of(self);

	// get io handle
	auto io = s3_get_io(s3);
	guard(guard_io, s3_io_free, io);

	// create bucket, if not exists
	s3_op_create_bucket(io, source);

	// put io back to cache
	unguard(&guard_io);
	cache_push(&s3->cache, &io->link);
}

static void
s3_detach(Cloud* self, Source* source)
{
	auto s3 = s3_of(self);

	// get io handle
	auto io = s3_get_io(s3);
	guard(guard_io, s3_io_free, io);

	// drop bucket
	s3_op_drop_bucket(io, source);

	// put io back to cache
	unguard(&guard_io);
	cache_push(&s3->cache, &io->link);
}

static void
s3_download(Cloud* self, Source* source, Id* id)
{
	auto s3 = s3_of(self);

	// create incomplete partition file
	char path[PATH_MAX];
	id_path(id, source, ID_INCOMPLETE, path);

	// in case previous attempt failed without crash
	if (fs_exists("%s", path))
		fs_unlink("%s", path);
	File file;
	file_init(&file);
	guard(guard_file, file_close, &file);
	file_create(&file, path);

	// get io handle
	auto io = s3_get_io(s3);
	guard(guard_io, s3_io_free, io);

	// execute GET and write content to the file
	s3_op_download(io, source, id, &file);

	// sync
	if (source->sync)
		file_sync(&file);

	// rename as completed
	char path_to[PATH_MAX];
	id_path(id, source, ID, path_to);
	fs_rename(path, "%s", path_to);

	// put io back to cache
	unguard(&guard_io);
	cache_push(&s3->cache, &io->link);
}

static void
s3_upload(Cloud* self, Source* source, Id* id)
{
	auto s3 = s3_of(self);

	// open partition file
	char path[PATH_MAX];
	id_path(id, source, ID, path);
	File file;
	file_init(&file);
	guard(guard_file, file_close, &file);
	file_open(&file, path);
	
	// get io handle
	auto io = s3_get_io(s3);
	guard(guard_io, s3_io_free, io);

	// execute PUT and transfer file content
	s3_op_upload(io, source, id, &file);

	// put io back to cache
	unguard(&guard_io);
	cache_push(&s3->cache, &io->link);
}

static void
s3_remove(Cloud* self, Source* source, Id* id)
{
	auto s3 = s3_of(self);
	
	// get io handle
	auto io = s3_get_io(s3);
	guard(guard_io, s3_io_free, io);

	// execute DELETE
	s3_op_delete(io, source, id);

	// put io back to cache
	unguard(&guard_io);
	cache_push(&s3->cache, &io->link);
}

static void
s3_read(Cloud* self, Source* source, Id* id, Buf* buf, uint32_t size,
        uint64_t offset)
{
	auto s3 = s3_of(self);
	
	// get io handle
	auto io = s3_get_io(s3);
	guard(guard_io, s3_io_free, io);

	// execute GET and read partial file range to the buffer
	s3_op_read(io, source, id, buf, size, offset);

	// put io back to cache
	unguard(&guard_io);
	cache_push(&s3->cache, &io->link);
}

CloudIf cloud_s3 =
{
	.create   = s3_create,
	.free     = s3_free,
	.attach   = s3_attach,
	.detach   = s3_detach,
	.download = s3_download,
	.upload   = s3_upload,
	.remove   = s3_remove,
	.read     = s3_read
};
