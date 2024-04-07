
//
// monotone.
//
// embeddable cloud-native storage for events
// and time-series data.
//
// Copyright (c) 2023-2024 Dmitry Simonenko
// Copyright (c) 2023-2024 Monotone
//
// Distributed under the terms of GNU LGPL v3 or
// Monotone Commercial License.
//

#include <monotone_runtime.h>
#include <monotone_lib.h>
#include <monotone_config.h>
#include <monotone_cloud.h>
#include <monotone_s3.h>

#include <openssl/hmac.h>
#include <openssl/evp.h>

#include <curl/curl.h>

static void
s3_request_encode_base64(uint8_t digest_raw[20],
                         uint8_t digest[28])
{
	static const char* const chars =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	unsigned long v = 0;
	int i = 0;
	int j = 0;
	for (; i < 20; ++i)
	{
		v = (v << 8) | digest_raw[i];
		if ((i % 3) == 2)
		{
			digest[j++] = chars[(v >> 18) & 0x3F];
			digest[j++] = chars[(v >> 12) & 0x3F];
			digest[j++] = chars[(v >> 6)  & 0x3F];
			digest[j++] = chars[v & 0x3F];
			v = 0;
		}
	}
	digest[j++] = chars[(v >> 10) & 0x3F];
	digest[j++] = chars[(v >> 4)  & 0x3F];
	digest[j++] = chars[(v << 2)  & 0x3F];
	digest[j++] = '=';
	digest[j++] = '\0';
}

static void
s3_request_prepare(S3Request* self)
{
	auto config = self->io->cloud->config;
	auto source = self->source;
	auto id     = self->id;

	// prepare RFC2616 date
	time_t    now = time(0);
	struct tm now_tm = *gmtime(&now);
	strftime(self->date, sizeof(self->date), "%a, %d %b %Y %H:%M:%S %Z", &now_tm);

	char uuid[UUID_SZ];
	uuid_to_string(&source->uuid, uuid, sizeof(uuid));

	// prepare request header
	int  header_size;
	char header[256];
	if (self->id)
	{
		header_size = snprintf(header, sizeof(header),
		                       "%s\n\n"
		                       "%s\n"
		                       "%s\n"
		                       "/%s/%020" PRIu64,
		                       self->method,
		                       self->content_type ? self->content_type : "",
		                       self->date,
		                       uuid,
		                       id->min);

		snprintf(self->url, sizeof(self->url),
		         "%.*s/%s/%020" PRIu64,
		         str_size(&config->url),
		         str_of(&config->url),
		         uuid,
		         id->min);
	} else
	{
		header_size = snprintf(header, sizeof(header),
		                       "%s\n\n"
		                       "%s\n"
		                       "%s\n"
		                       "/%s",
		                       self->method,
		                       self->content_type ? self->content_type : "",
		                       self->date,
		                       uuid);

		snprintf(self->url, sizeof(self->url),
		         "%.*s/%s",
		         str_size(&config->url),
		         str_of(&config->url),
		         uuid);
	}

	// create MAC (message authentication code)
	auto access_key = &config->login;
	auto secret_key = &config->password;

	uint8_t digest[20];
	HMAC(EVP_sha1(), str_of(secret_key), str_size(secret_key),
	     (unsigned char*)header, header_size,
	      digest, NULL);

	// encode MAC using base64
	uint8_t digest_base64[28];
	s3_request_encode_base64(digest, digest_base64);

	// prepare authorization header field
	snprintf(self->authorization, sizeof(self->authorization),
	         "AWS %.*s:%s",
	         str_size(access_key),
	         str_of(access_key),
	         digest_base64);
}

static int
s3_request_debug_cb(CURL*         handle,
                    curl_infotype type,
                    char*         data,
                    size_t        data_size,
                    void*         arg)
{
	unused(handle);

	const char* prefix;
	switch (type) {
	case CURLINFO_TEXT:
		prefix = "";
		break;
	case CURLINFO_HEADER_IN:
		prefix = "< ";
		break;
	case CURLINFO_HEADER_OUT:
		prefix = "> ";
		break;
	default:
		return 0;
	}
	if (data_size > 0) {
		if (data[data_size - 1] == '\n')
			data_size--;
	}

	CloudConfig* config = arg;
	log("s3 %.*s: %s%.*s",
	    str_size(&config->name),
	    str_of(&config->name),
	    prefix,
	    data_size, data);
	return 0;
}

void
s3_request_execute(S3Request* self)
{
	auto config = self->io->cloud->config;

	// prepare s3 request
	s3_request_prepare(self);

	// prepare http request headers
	struct curl_slist* headers = NULL;

	// date
	char data[256];
	snprintf(data, sizeof(data), "Date: %s", self->date);
	headers = curl_slist_append(headers, data);

	// content type
	if (self->content_type)
	{
		snprintf(data, sizeof(data), "Content-Type: %s", self->content_type);
		headers = curl_slist_append(headers, data);

		// content length
		snprintf(data, sizeof(data), "Content-Length: %" PRIu64, self->content_length);
		headers = curl_slist_append(headers, data);
	}

	// range
	if (self->range)
	{
		snprintf(data, sizeof(data), "Range: %s", self->range);
		headers = curl_slist_append(headers, data);
	}

	// authorization
	snprintf(data, sizeof(data), "Authorization: %s", self->authorization);
	headers = curl_slist_append(headers, data);

	// prepare and execute request
	CURL* curl = self->io->handle;
	curl_easy_reset(curl);
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, self->method);
	curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

	if (self->on_read)
	{
		if (self->content_length > 0)
		{
			curl_easy_setopt(curl, CURLOPT_UPLOAD, 1);
			curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, self->content_length);
		}
		curl_easy_setopt(curl, CURLOPT_READFUNCTION, self->on_read);
		curl_easy_setopt(curl, CURLOPT_READDATA, self->arg);
	}

	if (self->on_write)
	{
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, self->on_write);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, self->arg);
	}

	if (config->debug)
	{
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
		curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, s3_request_debug_cb);
		curl_easy_setopt(curl, CURLOPT_DEBUGDATA, config);
	}

	CURLcode code;
	code = curl_easy_setopt(curl, CURLOPT_URL, self->url);
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
	if (code == CURLE_HTTP_RETURNED_ERROR)
	{
		long http_code = 0;
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
		error("s3: HTTP %d (%s)",
		      str_size(&config->name),
		      str_of(&config->name),
		      (int)http_code,
	          str);
	}  else
	{
		error("s3 %.*s: %s",
		      str_size(&config->name),
		      str_of(&config->name),
		      str);
	}
}
