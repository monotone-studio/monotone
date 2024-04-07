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

typedef struct CompressionMgr CompressionMgr;

struct CompressionMgr
{
	Cache cache_zstd;
	Cache cache_lz4;
};

void compression_mgr_init(CompressionMgr*);
void compression_mgr_free(CompressionMgr*);
Compression*
compression_mgr_pop(CompressionMgr*, int);
void compression_mgr_push(CompressionMgr*, Compression*);
int  compression_mgr_of(Str*);
