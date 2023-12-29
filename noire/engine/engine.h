#pragma once

//
// noire
//
// time-series storage
//

typedef struct Engine Engine;

struct Engine
{
	Mutex       lock;
	PartTree    tree;
	List        list;
	int         list_count;
	TierMgr     tier_mgr;
	LockMgr     lock_mgr;
	Service*    service;
	Comparator* comparator;
};

void engine_init(Engine*, Comparator*, Service*, StorageMgr*);
void engine_free(Engine*);
void engine_open(Engine*);
void engine_close(Engine*);
void engine_flush(Engine*);
Lock*
engine_find(Engine*, bool, uint64_t);
