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
void storage_mgr_print(StorageMgr*, Buf*);
Storage*
storage_mgr_find(StorageMgr*, Str*);
Part*
storage_mgr_find_part(StorageMgr*, uint64_t);
