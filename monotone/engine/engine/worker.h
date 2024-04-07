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
