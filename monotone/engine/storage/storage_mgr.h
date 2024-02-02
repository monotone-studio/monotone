#pragma once

//
// monotone
//
// time-series storage
//

typedef struct StorageMgr StorageMgr;

struct StorageMgr
{
	List      list;
	int       list_count;
	CloudMgr* cloud_mgr;
};

void storage_mgr_init(StorageMgr*, CloudMgr*);
void storage_mgr_free(StorageMgr*);
void storage_mgr_open(StorageMgr*);
void storage_mgr_close(StorageMgr*);
bool storage_mgr_create_system(StorageMgr*);
bool storage_mgr_create(StorageMgr*, Source*, bool);
void storage_mgr_drop(StorageMgr*, Str*, bool);
void storage_mgr_alter(StorageMgr*, Source*, int, bool);
void storage_mgr_rename(StorageMgr*, Str*, Str*, bool);
void storage_mgr_show(StorageMgr*, Str*, Buf*);
void storage_mgr_show_partitions(StorageMgr*, Str*, Buf*);
Storage*
storage_mgr_find(StorageMgr*, Str*);

static inline Storage*
storage_mgr_first(StorageMgr* self)
{
	assert(self->list_count > 0);
	return container_of(self->list.next, Storage, link);
}
