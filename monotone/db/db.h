#pragma once

//
// monotone
//
// time-series storage
//

typedef struct Db Db;

struct Db
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

void  db_init(Db*, Comparator*);
void  db_free(Db*);
void  db_open(Db*);
void  db_close(Db*);
Lock* db_find(Db*, bool, uint64_t);
Lock* db_seek(Db*, uint64_t);
Lock* db_seek_next(Db*, uint64_t);
