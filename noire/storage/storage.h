#pragma once

//
// noire
//
// time-series storage
//

typedef struct Storage Storage;

struct Storage
{
	Engine        engine;
	Service       service;
	CompactionMgr compaction_mgr;
	Comparator    comparator;
	Config        config;
	Global        global;
};

void storage_init(Storage*);
void storage_free(Storage*);
void storage_start(Storage*);
void storage_stop(Storage*);
void storage_insert(Storage*, uint64_t, void*, int);
