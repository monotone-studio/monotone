#pragma once

//
// monotone
//
// time-series storage
//

typedef struct Engine Engine;

struct Engine
{
	Mutex       lock;
	LockMgr     lock_mgr;
	PartTree    tree;
	atomic_u64  rows_written;
	atomic_u64  rows_written_bytes;
	Service     service;
	Conveyor    conveyor;
	StorageMgr  storage_mgr;
	Comparator* comparator;
};

void  engine_init(Engine*, Comparator*);
void  engine_free(Engine*);
void  engine_open(Engine*);
void  engine_close(Engine*);
Lock* engine_find(Engine*, bool, uint64_t);
Lock* engine_seek(Engine*, uint64_t);
Lock* engine_seek_next(Engine*, uint64_t);
