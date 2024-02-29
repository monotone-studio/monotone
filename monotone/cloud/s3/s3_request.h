#pragma once

//
// monotone
//
// time-series storage
//

typedef struct S3Request S3Request;

struct S3Request
{
	S3Io*       io;
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
};

void s3_request_execute(S3Request*);
