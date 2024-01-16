#pragma once

//
// monotone
//
// time-series storage
//

typedef struct Compaction Compaction;

struct Compaction
{
	ServicePart* ref;
	ServiceReq*  req;
	Part*        origin;
	Part*        part;
	Memtable*    memtable;
	Storage*     storage_origin;
	Storage*     storage;
	Merger       merger;
	Engine*      engine;
};

void compaction_init(Compaction*, Engine*);
void compaction_free(Compaction*);
void compaction_reset(Compaction*);
void compaction_run(Compaction*, ServicePart*);
