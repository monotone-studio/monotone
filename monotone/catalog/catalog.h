#pragma once

//
// monotone
//
// time-series storage
//

typedef struct Catalog Catalog;

struct Catalog
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

void catalog_init(Catalog*, Comparator*, Service*);
void catalog_free(Catalog*);
void catalog_open(Catalog*);
void catalog_close(Catalog*);
void catalog_lock_global(Catalog*, bool);
void catalog_unlock_global(Catalog*);
Ref* catalog_lock(Catalog*, uint64_t, int, bool, bool);
void catalog_unlock(Catalog*, Ref*, int);

void catalog_drop(Catalog*, uint64_t, bool);
bool catalog_rebalance(Catalog*, uint64_t*, Str*);
void catalog_checkpoint(Catalog*);
