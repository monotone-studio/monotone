#pragma once

//
// monotone
//
// time-series storage
//

typedef struct Compaction Compaction;

struct Compaction
{
	Merger   merger;
	Engine*  engine;
	Service* service;
	void*    global;
	Thread   thread;
};

void compaction_init(Compaction*);
void compaction_free(Compaction*);
void compaction_start(Compaction*, Service*, Engine*);
void compaction_stop(Compaction*);
void compaction_execute(Compaction*, ServiceReq*, Tier*);
void compaction_rebalance(Compaction*);
