#pragma once

//
// monotone
//
// time-series storage
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
