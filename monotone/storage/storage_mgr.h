#pragma once

//
// monotone
//
// time-series storage
//

typedef struct StorageMgr StorageMgr;

struct StorageMgr
{
	List list;
	int  list_count;
};

void storage_mgr_init(StorageMgr*);
void storage_mgr_free(StorageMgr*);
void storage_mgr_open(StorageMgr*);
void storage_mgr_create(StorageMgr*, Target*, bool);
void storage_mgr_drop(StorageMgr*, Str*, bool);
Storage*
storage_mgr_find(StorageMgr*, Str*);
