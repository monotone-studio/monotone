#pragma once

//
// monotone
//
// time-series storage
//

typedef struct WorkerMgr WorkerMgr;

struct WorkerMgr
{
	int     workers_count;
	Worker* workers;
};

void worker_mgr_init(WorkerMgr*);
void worker_mgr_start(WorkerMgr*, Service*, Engine*);
void worker_mgr_stop(WorkerMgr*);
