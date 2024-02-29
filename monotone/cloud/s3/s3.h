#pragma once

//
// monotone
//
// time-series storage
//

typedef struct S3 S3;

struct S3
{
	Cloud cloud;
	Cache cache;
};

always_inline static inline S3*
s3_of(Cloud* self)
{
	return (S3*)self;
}

extern CloudIf cloud_s3;
