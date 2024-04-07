#pragma once

//
// monotone.
//
// embeddable cloud-native storage for events
// and time-series data.
//
// Copyright (c) 2023-2024 Dmitry Simonenko
// Copyright (c) 2023-2024 Monotone
//
// Distributed under the terms of GNU LGPL v3 or
// Monotone Commercial License.
//

typedef struct WorkerMgr WorkerMgr;

struct WorkerMgr
{
	int     workers_count;
	Worker* workers;
};

void worker_mgr_init(WorkerMgr*);
void worker_mgr_start(WorkerMgr*, Engine*);
void worker_mgr_stop(WorkerMgr*);
