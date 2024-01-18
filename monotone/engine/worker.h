#pragma once

//
// monotone
//
// time-series storage
//

typedef struct Worker Worker;

struct Worker
{
	Compaction compaction;
	Engine*    engine;
	Context*   context;
	Thread     thread;
};

void worker_init(Worker*, Engine*);
void worker_free(Worker*);
void worker_start(Worker*);
void worker_stop(Worker*);
