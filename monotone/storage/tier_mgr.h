#pragma once

//
// monotone
//
// time-series tier
//

typedef struct TierMgr TierMgr;

struct TierMgr
{
	List        list;
	int         list_count;
	StorageMgr* storage_mgr;
};

void tier_mgr_init(TierMgr*, StorageMgr*);
void tier_mgr_free(TierMgr*);
void tier_mgr_open(TierMgr*);
void tier_mgr_create(TierMgr*, TierConfig*, bool);
void tier_mgr_drop(TierMgr*, Str*, bool);
Tier*
tier_mgr_find(TierMgr*, Str*);
