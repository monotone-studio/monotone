#pragma once

//
// monotone
//
// time-series storage
//

typedef struct S3Io S3Io;

typedef size_t (*S3Function)(void* ptr, size_t len, size_t nmemb, void* arg);

struct S3Io
{
	void*  handle;
	Cloud* cloud;
	List   link;
};

S3Io* s3_io_allocate(Cloud*);
void  s3_io_free(S3Io*);
