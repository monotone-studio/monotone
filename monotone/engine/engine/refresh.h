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

typedef struct Refresh Refresh;

struct Refresh
{
	Ref*      ref;
	Part*     origin;
	Part*     part;
	Memtable* memtable;
	Storage*  storage_origin;
	Storage*  storage;
	Merger    merger;
	Engine*   engine;
};

void refresh_init(Refresh*, Engine*);
void refresh_free(Refresh*);
void refresh_reset(Refresh*);
void refresh_run(Refresh*, uint64_t, Str*, bool);
