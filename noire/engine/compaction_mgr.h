#pragma once

//
// noire
//
// time-series storage
//

typedef struct CompactionMgr CompactionMgr;

struct CompactionMgr
{
	int         workers_count;
	Compaction* workers;
};

void compaction_mgr_init(CompactionMgr*);
void compaction_mgr_start(CompactionMgr*, Service*, Engine*);
void compaction_mgr_stop(CompactionMgr*);
