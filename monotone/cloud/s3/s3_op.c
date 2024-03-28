
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

static size_t
s3_op_download_cb(void* ptr, size_t len, size_t nmemb, void* arg)
{
	File* file = arg;
	auto rc = file_write_nothrow(file, ptr, len * nmemb);
	if (unlikely(rc == -1))
		return 0;
	return rc;
}

void
s3_op_download(S3Io* self, Source* source, Id* id, File* file)
{
	S3Request req =
	{
		.io               = self,
		.on_read          = NULL,
		.on_write         = s3_op_download_cb,
		.arg              = file,
		.source           = source,
		.id               = id,
		.method           = "GET",
		.content_type     = NULL,
		.content_length   = 0,
		.range            = NULL,
		.date[0]          = 0,
		.authorization[0] = 0,
		.url[0]           = 0
	};
	s3_request_execute(&req);
}

static size_t
s3_op_upload_cb(void* ptr, size_t len, size_t nmemb, void* arg)
{
	S3Upload* upload = arg;

	size_t left = upload->file->size - upload->file_offset;
	size_t size_limit = len * nmemb;
	size_t size;
	if (left < size_limit)
		size = left;
	else
		size = size_limit;

	int rc = vfs_read(upload->file->fd, ptr, size);
	if (rc == -1)
		return 0;

	upload->file_offset += size;
	return size;
}

void
s3_op_upload(S3Io* self, Source* source, Id* id, File* file)
{
	S3Upload upload =
	{
		.file        = file,
		.file_offset = 0,
		.io          = self
	};
	S3Request req =
	{
		.io               = self,
		.on_read          = s3_op_upload_cb,
		.on_write         = NULL,
		.arg              = &upload,
		.source           = source,
		.id               = id,
		.method           = "PUT",
		.content_type     = "application/octet-stream",
		.content_length   = file->size,
		.range            = NULL,
		.date[0]          = 0,
		.authorization[0] = 0,
		.url[0]           = 0
	};
	s3_request_execute(&req);
}

void
s3_op_delete(S3Io* self, Source* source, Id* id)
{
	S3Request req =
	{
		.io               = self,
		.on_read          = NULL,
		.on_write         = NULL,
		.arg              = NULL,
		.source           = source,
		.id               = id,
		.method           = "DELETE",
		.content_type     = NULL,
		.content_length   = 0,
		.range            = NULL,
		.date[0]          = 0,
		.authorization[0] = 0,
		.url[0]           = 0
	};
	s3_request_execute(&req);
}

hot static size_t
s3_op_read_cb(void* ptr, size_t len, size_t nmemb, void* arg)
{
	Buf* buf = arg;
	size_t read = len * nmemb;
	Exception e;
	if (try(&e))
		buf_write(buf, ptr, len * nmemb);
	if (catch(&e))
		read = 0;
	return read;
}

void
s3_op_read(S3Io*    self,
           Source*  source,
           Id*      id,
           Buf*     buf,
           uint32_t size,
           uint64_t offset)
{
	char range[128];
	snprintf(range, sizeof(range), "bytes=%" PRIu64 "-%" PRIu64,
	         offset, offset + (size - 1));
	S3Request req =
	{
		.io               = self,
		.on_read          = NULL,
		.on_write         = s3_op_read_cb,
		.arg              = buf,
		.source           = source,
		.id               = id,
		.method           = "GET",
		.content_type     = NULL,
		.content_length   = 0,
		.range            = range,
		.date[0]          = 0,
		.authorization[0] = 0,
		.url[0]           = 0
	};
	s3_request_execute(&req);
}

void
s3_op_create_bucket(S3Io* self, Source* source)
{
	S3Request req =
	{
		.io               = self,
		.on_read          = NULL,
		.on_write         = NULL,
		.arg              = NULL,
		.source           = source,
		.id               = NULL,
		.method           = "PUT",
		.content_type     = "application/octet-stream",
		.content_length   = 0,
		.range            = NULL,
		.date[0]          = 0,
		.authorization[0] = 0,
		.url[0]           = 0
	};
	s3_request_execute(&req);
}

void
s3_op_drop_bucket(S3Io* self, Source* source)
{
	S3Request req =
	{
		.io               = self,
		.on_read          = NULL,
		.on_write         = NULL,
		.arg              = NULL,
		.source           = source,
		.id               = NULL,
		.method           = "DELETE",
		.content_type     = NULL,
		.content_length   = 0,
		.range            = NULL,
		.date[0]          = 0,
		.authorization[0] = 0,
		.url[0]           = 0
	};
	s3_request_execute(&req);
}
