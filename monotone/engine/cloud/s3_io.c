
//
// monotone
//
// time-series storage
//

#include <monotone_runtime.h>
#include <monotone_lib.h>
#include <monotone_config.h>
#include <monotone_cloud.h>

#include <openssl/hmac.h>
#include <openssl/evp.h>

#include <curl/curl.h>

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

static inline void
s3_get_date(char* buf, int buf_size)
{
	time_t now = time(0);
	struct tm tm;
	tm = *gmtime(&now);
	strftime(buf, buf_size, "%a, %d %b %Y %H:%M:%S %Z", &tm);
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
	auto config = self->cloud->config;

	// date
	char date[128];
	s3_get_date(date, sizeof(date));

	// prepare header
	int  data_size;
	char data[256];
	data_size = snprintf(data, sizeof(data),
	                     "GET\n\n"
	                     "application/octet-stream\n"
	                     "%s\n"
	                     "/%.*s/%020" PRIu64, date,
	                     str_size(&source->name),
	                     str_of(&source->name),
	                     id->min);

	// sign header using base64 hmac
	auto access_key = &config->login;
	auto secret_key = &config->password;

	char hmac[HMAC_SZ];
	hmac_base64(hmac, sizeof(hmac),
	            str_of(secret_key),
	            str_size(secret_key),
	            data,
	            data_size);

	// content type
	struct curl_slist* headers = NULL;
	snprintf(data, sizeof(data), "Content-Type: application/octet-stream");
	headers = curl_slist_append(headers, data);

	// date type
	snprintf(data, sizeof(data), "Date: %s", date);
	headers = curl_slist_append(headers, data);

	// authorization
	snprintf(data, sizeof(data), "Authorization: AWS %.*s:%s",
	         str_size(access_key),
	         str_of(access_key),
	         hmac);
	headers = curl_slist_append(headers, data);

	// url
	char url[256];
	snprintf(url, sizeof(url), "%.*s/%.*s/%020" PRIu64,
	         str_size(&config->url),
	         str_of(&config->url),
	         str_size(&source->name),
	         str_of(&source->name),
	         id->min);

	// prepare and execute request
	CURL* curl = self->handle;
	curl_easy_reset(curl);

	if (config->debug)
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");
	curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, s3_io_download_cb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
	curl_easy_setopt(curl, CURLOPT_URL, url);
	auto ret = curl_easy_perform(curl);
	curl_slist_free_all(headers);

	if (ret != CURLE_OK)
	{
		const char* str = curl_easy_strerror(ret);
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
}

typedef struct S3Upload S3Upload;

struct S3Upload
{
	uint64_t offset;
	File*    file;
	S3Io*    io;
};

static size_t
s3_io_upload_cb(void* ptr, size_t len, size_t nmemb, void* arg)
{
	S3Upload* upload = arg;

	size_t left = upload->file->size - upload->offset;
	size_t size_limit = len * nmemb;
	size_t size;
	if (left < size_limit)
		size = left;
	else
		size = size_limit;

	int rc = vfs_read(upload->file->fd, ptr, size);
	if (rc == -1)
		return 0;

	upload->offset += size;
	return size;
}

void
s3_io_upload(S3Io* self, Source* source, Id* id, File* file)
{
	auto config = self->cloud->config;

	// date
	char date[128];
	s3_get_date(date, sizeof(date));

	// prepare header
	int  data_size;
	char data[256];
	data_size = snprintf(data, sizeof(data),
	                     "PUT\n\n"
	                     "application/octet-stream\n"
	                     "%s\n"
	                     "/%.*s/%020" PRIu64, date,
	                     str_size(&source->name),
	                     str_of(&source->name),
	                     id->min);

	// sign header using base64 hmac
	auto access_key = &config->login;
	auto secret_key = &config->password;

	char hmac[HMAC_SZ];
	hmac_base64(hmac, sizeof(hmac),
	            str_of(secret_key),
	            str_size(secret_key),
	            data,
	            data_size);

	// content type
	struct curl_slist* headers = NULL;
	snprintf(data, sizeof(data), "Content-Type: application/octet-stream");
	headers = curl_slist_append(headers, data);

	// date type
	snprintf(data, sizeof(data), "Date: %s", date);
	headers = curl_slist_append(headers, data);

	// authorization
	snprintf(data, sizeof(data), "Authorization: AWS %.*s:%s",
	         str_size(access_key),
	         str_of(access_key),
	         hmac);
	headers = curl_slist_append(headers, data);

	// content length
	snprintf(data, sizeof(data), "Content-Length: %" PRIu64, file->size);
	headers = curl_slist_append(headers, data);

	// url
	char url[256];
	snprintf(url, sizeof(url), "%.*s/%.*s/%020" PRIu64,
	         str_size(&config->url),
	         str_of(&config->url),
	         str_size(&source->name),
	         str_of(&source->name),
	         id->min);

	// prepare and execute request
	S3Upload upload =
	{
		.offset = 0,
		.file   = file,
		.io     = self
	};
	curl_off_t size = file->size;
	CURL* curl = self->handle;
	curl_easy_reset(curl);

	if (config->debug)
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
	curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

	curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, size);
	curl_easy_setopt(curl, CURLOPT_UPLOAD, 1);
	curl_easy_setopt(curl, CURLOPT_READFUNCTION, s3_io_upload_cb);
	curl_easy_setopt(curl, CURLOPT_READDATA, &upload);

	curl_easy_setopt(curl, CURLOPT_URL, url);

	auto ret = curl_easy_perform(curl);
	curl_slist_free_all(headers);

	if (ret != CURLE_OK)
	{
		const char* str = curl_easy_strerror(ret);
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
}

void
s3_io_delete(S3Io* self, Source* source, Id* id)
{
	auto config = self->cloud->config;

	// date
	char date[128];
	s3_get_date(date, sizeof(date));

	// prepare header
	int  data_size;
	char data[256];
	data_size = snprintf(data, sizeof(data),
	                     "DELETE\n\n"
	                     "\n"
	                     "%s\n"
	                     "/%.*s/%020" PRIu64, date,
	                     str_size(&source->name),
	                     str_of(&source->name),
	                     id->min);

	// sign header using base64 hmac
	auto access_key = &config->login;
	auto secret_key = &config->password;

	char hmac[HMAC_SZ];
	hmac_base64(hmac, sizeof(hmac),
	            str_of(secret_key),
	            str_size(secret_key),
	            data,
	            data_size);

	// date type
	struct curl_slist* headers = NULL;
	snprintf(data, sizeof(data), "Date: %s", date);
	headers = curl_slist_append(headers, data);

	// authorization
	snprintf(data, sizeof(data), "Authorization: AWS %.*s:%s",
	         str_size(access_key),
	         str_of(access_key),
	         hmac);
	headers = curl_slist_append(headers, data);

	// url
	char url[256];
	snprintf(url, sizeof(url), "%.*s/%.*s/%020" PRIu64,
	         str_size(&config->url),
	         str_of(&config->url),
	         str_size(&source->name),
	         str_of(&source->name),
	         id->min);

	// prepare and execute request
	CURL* curl = self->handle;
	curl_easy_reset(curl);

	if (config->debug)
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
	curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_URL, url);
	auto ret = curl_easy_perform(curl);
	curl_slist_free_all(headers);

	if (ret != CURLE_OK)
	{
		const char* str = curl_easy_strerror(ret);
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
}

static size_t
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
	auto config = self->cloud->config;

	// date
	char date[128];
	s3_get_date(date, sizeof(date));

	// prepare header
	int  data_size;
	char data[256];
	data_size = snprintf(data, sizeof(data),
	                     "GET\n\n"
	                     "application/octet-stream\n"
	                     "%s\n"
	                     "/%.*s/%020" PRIu64, date,
	                     str_size(&source->name),
	                     str_of(&source->name),
	                     id->min);

	// sign header using base64 hmac
	auto access_key = &config->login;
	auto secret_key = &config->password;

	char hmac[HMAC_SZ];
	hmac_base64(hmac, sizeof(hmac),
	            str_of(secret_key),
	            str_size(secret_key),
	            data,
	            data_size);

	// content type
	struct curl_slist* headers = NULL;
	snprintf(data, sizeof(data), "Content-Type: application/octet-stream");
	headers = curl_slist_append(headers, data);

	// range
	snprintf(data, sizeof(data), "Range: bytes=%" PRIu64 "-%" PRIu64, offset, offset + size);
	headers = curl_slist_append(headers, data);

	// date type
	snprintf(data, sizeof(data), "Date: %s", date);
	headers = curl_slist_append(headers, data);

	// authorization
	snprintf(data, sizeof(data), "Authorization: AWS %.*s:%s",
	         str_size(access_key),
	         str_of(access_key),
	         hmac);
	headers = curl_slist_append(headers, data);

	// url
	char url[256];
	snprintf(url, sizeof(url), "%.*s/%.*s/%020" PRIu64,
	         str_size(&config->url),
	         str_of(&config->url),
	         str_size(&source->name),
	         str_of(&source->name),
	         id->min);

	// prepare and execute request
	CURL* curl = self->handle;
	curl_easy_reset(curl);

	if (config->debug)
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");
	curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
	/*curl_easy_setopt(curl, CURLOPT_RANGE, data);*/
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, s3_io_read_cb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, buf);
	curl_easy_setopt(curl, CURLOPT_URL, url);
	auto ret = curl_easy_perform(curl);
	curl_slist_free_all(headers);

	if (ret != CURLE_OK)
	{
		const char* str = curl_easy_strerror(ret);
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
}

void
s3_io_create_bucket(S3Io* self, Source* source)
{
	auto config = self->cloud->config;

	// date
	char date[128];
	s3_get_date(date, sizeof(date));

	// prepare header
	int  data_size;
	char data[256];
	data_size = snprintf(data, sizeof(data),
	                     "PUT\n\n"
	                     "\n"
	                     "%s\n"
	                     "/%.*s", date,
	                     str_size(&source->name),
	                     str_of(&source->name));

	// sign header using base64 hmac
	auto access_key = &config->login;
	auto secret_key = &config->password;

	char hmac[HMAC_SZ];
	hmac_base64(hmac, sizeof(hmac),
	            str_of(secret_key),
	            str_size(secret_key),
	            data,
	            data_size);

	// date type
	struct curl_slist* headers = NULL;
	snprintf(data, sizeof(data), "Date: %s", date);
	headers = curl_slist_append(headers, data);

	// authorization
	snprintf(data, sizeof(data), "Authorization: AWS %.*s:%s",
	         str_size(access_key),
	         str_of(access_key),
	         hmac);
	headers = curl_slist_append(headers, data);

	// content length
	snprintf(data, sizeof(data), "Content-Length: 0");
	headers = curl_slist_append(headers, data);

	// url
	char url[256];
	snprintf(url, sizeof(url), "%.*s/%.*s",
	         str_size(&config->url),
	         str_of(&config->url),
	         str_size(&source->name),
	         str_of(&source->name));

	// prepare and execute request
	CURL* curl = self->handle;
	curl_easy_reset(curl);

	if (config->debug)
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
	curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_URL, url);
	auto ret = curl_easy_perform(curl);
	curl_slist_free_all(headers);

	if (ret != CURLE_OK)
	{
		const char* str = curl_easy_strerror(ret);
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
}
