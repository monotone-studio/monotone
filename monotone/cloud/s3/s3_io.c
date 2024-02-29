
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
