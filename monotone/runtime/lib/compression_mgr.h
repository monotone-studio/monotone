#pragma once

//
// monotone
//
// time-series storage
//

typedef struct CompressionMgr CompressionMgr;

struct CompressionMgr
{
	CompressionCache* cache;
};

void compression_mgr_init(CompressionMgr*);
void compression_mgr_free(CompressionMgr*);
void compression_mgr_create(CompressionMgr*);
Compression*
compression_mgr_pop(CompressionMgr*, int);
void compression_mgr_push(CompressionMgr*, Compression*);
int  compression_mgr_of(Str*);
