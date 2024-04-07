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

typedef struct S3Upload S3Upload;

struct S3Upload
{
	File*    file;
	uint64_t file_offset;
	S3Io*    io;
};

void s3_op_download(S3Io*, Source*, Id*, File*);
void s3_op_upload(S3Io*, Source*, Id*, File*);
void s3_op_delete(S3Io*, Source*, Id*);
void s3_op_read(S3Io*, Source*, Id*, Buf*, uint32_t, uint64_t);
void s3_op_create_bucket(S3Io*, Source*);
void s3_op_drop_bucket(S3Io*, Source*);
