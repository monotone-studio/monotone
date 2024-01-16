#pragma once

//
// monotone
//
// time-series storage
//

typedef struct Worker Worker;

struct Worker
{
	Merger   merger;
	Engine*  engine;
	Service* service;
	Context* context;
	Thread   thread;
};

void worker_init(Worker*);
void worker_free(Worker*);
void worker_start(Worker*, Service*, Engine*);
void worker_stop(Worker*);
