#pragma once

//
// monotone
//
// time-series storage
//

typedef struct Worker Worker;

struct Worker
{
	Refresh       refresh;
	Engine*       engine;
	ServiceFilter filter;
	Context*      context;
	Thread        thread;
};

void worker_init(Worker*, Engine*, ServiceFilter);
void worker_free(Worker*);
void worker_start(Worker*);
void worker_stop(Worker*);
