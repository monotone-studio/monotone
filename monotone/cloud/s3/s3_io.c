
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

#include <openssl/hmac.h>
#include <openssl/evp.h>

#include <curl/curl.h>

typedef size_t (*S3Function)(void* ptr, size_t len, size_t nmemb, void* arg);

typedef struct
{
	S3Function  on_read;
	S3Function  on_write;
	void*       arg;
	Source*     source;
	Id*         id;
	const char* method;
	const char* content_type;
	uint64_t    content_length;
	const char* range;
	char        date[64];
	char        authorization[256];
	char        url[1024];
} S3Request;

static inline void
s3_io_request(S3Request* self, CloudConfig* config)
{
	auto source = self->source;
	auto id     = self->id;

	// prepare RFC2616 date
	time_t    now = time(0);
	struct tm now_tm = *gmtime(&now);
	strftime(self->date, sizeof(self->date), "%a, %d %b %Y %H:%M:%S %Z", &now_tm);

	// prepare request header
	int  header_size;
	char header[256];
	if (self->id)
	{
		header_size = snprintf(header, sizeof(header),
		                       "%s\n\n"
		                       "%s\n"
		                       "%s\n"
		                       "/%.*s/%020" PRIu64,
		                       self->method,
		                       self->content_type ? self->content_type : "",
		                       self->date,
		                       str_size(&source->name),
		                       str_of(&source->name),
		                       id->min);

		snprintf(self->url, sizeof(self->url),
		        "%.*s/%.*s/%020" PRIu64,
		         str_size(&config->url),
		         str_of(&config->url),
		         str_size(&source->name),
		         str_of(&source->name),
		         id->min);
	} else
	{
		header_size = snprintf(header, sizeof(header),
		                       "%s\n\n"
		                       "%s\n"
		                       "%s\n"
		                       "/%.*s",
		                       self->method,
		                       self->content_type ? self->content_type : "",
		                       self->date,
		                       str_size(&source->name),
		                       str_of(&source->name));

		snprintf(self->url, sizeof(self->url),
		         "%.*s/%.*s",
		         str_size(&config->url),
		         str_of(&config->url),
		         str_size(&source->name),
		         str_of(&source->name));
	}

	// sign header using base64 hmac
	auto access_key = &config->login;
	auto secret_key = &config->password;

	char hmac[HMAC_SZ];
	hmac_base64(hmac, sizeof(hmac),
	            str_of(secret_key),
	            str_size(secret_key),
	            header,
	            header_size);

	// create authorization header field
	snprintf(self->authorization, sizeof(self->authorization),
	         "AWS %.*s:%s",
	         str_size(access_key),
	         str_of(access_key),
	         hmac);
}

static inline void
s3_io_execute(S3Io* self, S3Request* req)
{
	// prepare s3 request
	s3_io_request(req, self->cloud->config);

	// prepare http request headers
	struct curl_slist* headers = NULL;

	// date
	char data[256];
	snprintf(data, sizeof(data), "Date: %s", req->date);
	headers = curl_slist_append(headers, data);

	// content type
	if (req->content_type)
	{
		snprintf(data, sizeof(data), "Content-Type: %s", req->content_type);
		headers = curl_slist_append(headers, data);

		// content length
		snprintf(data, sizeof(data), "Content-Length: %" PRIu64, req->content_length);
		headers = curl_slist_append(headers, data);
	}

	// range
	if (req->range)
	{
		snprintf(data, sizeof(data), "Range: %s", req->range);
		headers = curl_slist_append(headers, data);
	}

	// authorization
	snprintf(data, sizeof(data), "Authorization: %s", req->authorization);
	headers = curl_slist_append(headers, data);

	// prepare and execute request
	CURL* curl = self->handle;
	curl_easy_reset(curl);
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, req->method);
	curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	if (req->on_read)
	{
		if (req->content_length > 0)
		{
			curl_easy_setopt(curl, CURLOPT_UPLOAD, 1);
			curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, req->content_length);
		}
		curl_easy_setopt(curl, CURLOPT_READFUNCTION, req->on_read);
		curl_easy_setopt(curl, CURLOPT_READDATA, req->arg);
	}
	if (req->on_write)
	{
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, req->on_write);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, req->arg);
	}
	if (self->cloud->config->debug)
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);

	CURLcode code;
	code = curl_easy_setopt(curl, CURLOPT_URL, req->url);
	if (code != CURLE_OK)
		goto error;

	code = curl_easy_perform(curl);
	if (code != CURLE_OK)
		goto error;

	curl_slist_free_all(headers);
	return;

error:;
	curl_slist_free_all(headers);

	const char* str = curl_easy_strerror(code);
	if (CURLE_HTTP_RETURNED_ERROR)
	{
		long http_code = 0;
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
		error("s3: HTTP %d (%s)", (int)http_code, str);
	}  else
	{
		error("s3: %s", str);
	}
}

S3Io*
s3_io_allocate(Cloud* cloud)
{
	auto self = (S3Io*)mn_malloc(sizeof(S3Io));
	list_init(&self->link);
	self->cloud  = cloud;
	self->handle = curl_easy_init();
	if (self->handle == NULL)
	{
		mn_free(self);
		error_system();
	}
	return self;
}

void
s3_io_free(S3Io* self)
{
	if (self->handle)
		curl_easy_cleanup(self->handle);
	mn_free(self);
}

static size_t
s3_io_download_cb(void* ptr, size_t len, size_t nmemb, void* arg)
{
	File* file = arg;
	auto rc = file_write_nothrow(file, ptr, len * nmemb);
	if (unlikely(rc == -1))
		return 0;
	return rc;
}

void
s3_io_download(S3Io* self, Source* source, Id* id, File* file)
{
	S3Request req =
	{
		.on_read          = NULL,
		.on_write         = s3_io_download_cb,
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
	s3_io_execute(self, &req);
}

typedef struct S3Upload S3Upload;

struct S3Upload
{
	File*    file;
	uint64_t file_offset;
	S3Io*    io;
};

static size_t
s3_io_upload_cb(void* ptr, size_t len, size_t nmemb, void* arg)
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
s3_io_upload(S3Io* self, Source* source, Id* id, File* file)
{
	S3Upload upload =
	{
		.file        = file,
		.file_offset = 0,
		.io          = self
	};
	S3Request req =
	{
		.on_read          = s3_io_upload_cb,
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
	s3_io_execute(self, &req);
}

void
s3_io_delete(S3Io* self, Source* source, Id* id)
{
	S3Request req =
	{
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
	s3_io_execute(self, &req);
}

hot static size_t
s3_io_read_cb(void* ptr, size_t len, size_t nmemb, void* arg)
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
s3_io_read(S3Io*    self,
           Source*  source,
           Id*      id,
           Buf*     buf,
           uint32_t size,
           uint64_t offset)
{
	char range[128];
	snprintf(range, sizeof(range), "bytes=%" PRIu64 "-%" PRIu64,
	         offset, offset + size);
	S3Request req =
	{
		.on_read          = NULL,
		.on_write         = s3_io_read_cb,
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
	s3_io_execute(self, &req);
}

void
s3_io_create_bucket(S3Io* self, Source* source)
{
	S3Request req =
	{
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
	s3_io_execute(self, &req);
}
