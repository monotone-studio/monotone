#pragma once

//
// monotone
//
// time-series storage
//

typedef struct S3Io S3Io;

struct S3Io
{
	void*  handle;
	Cloud* cloud;
	List   link;
};

S3Io* s3_io_allocate(Cloud*);
void  s3_io_free(S3Io*);
void  s3_io_download(S3Io*, Source*, Id*, File*);
void  s3_io_upload(S3Io*, Source*, Id*, File*);
void  s3_io_delete(S3Io*, Source*, Id*);
void  s3_io_read(S3Io*, Source*, Id*, Buf*, uint32_t, uint64_t);
void  s3_io_create_bucket(S3Io*, Source*);
