#pragma once

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
