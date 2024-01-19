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
	Rwlock      lock_global;
	Mutex       lock;
	CondVar     cond_var;
	// partition mapping
	Mapping     mapping;
	// objects
	Conveyor    conveyor;
	StorageMgr  storage_mgr;
	Service*    service;
	Comparator* comparator;
};

void engine_init(Engine*, Comparator*, Service*);
void engine_free(Engine*);
void engine_open(Engine*);
void engine_close(Engine*);

void engine_lock_global(Engine*, bool);
void engine_unlock_global(Engine*);
Ref* engine_lock(Engine*, uint64_t, RefLock, bool, bool);
void engine_unlock(Engine*, Ref*, RefLock);
