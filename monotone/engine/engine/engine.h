#pragma once

//
// monotone
//
// time-series storage
//

typedef struct Engine Engine;

struct Engine
{
	// locking
	Mutex       lock;
	CondVar     cond_var;
	// partition mapping
	Mapping     mapping;
	// objects
	Conveyor    conveyor;
	StorageMgr  storage_mgr;
	Wal*        wal;
	Service*    service;
	Comparator* comparator;
};

void engine_init(Engine*, Comparator*, Wal*, Service*, CloudMgr*);
void engine_free(Engine*);
void engine_open(Engine*);
void engine_close(Engine*);
void engine_set_serial(Engine*);
Ref* engine_lock(Engine*, uint64_t, LockType, bool, bool);
void engine_unlock(Engine*, Ref*, LockType);
