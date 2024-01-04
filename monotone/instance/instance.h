#pragma once

//
// monotone
//
// time-series storage
//

typedef struct Instance Instance;

struct Instance
{
	Engine        engine;
	Service       service;
	CompactionMgr compaction_mgr;
	Comparator    comparator;
	StorageMgr    storage_mgr;
	Config        config;
	Global        global;
};

void instance_init(Instance*);
void instance_free(Instance*);
void instance_start(Instance*, const char*);
void instance_stop(Instance*);
