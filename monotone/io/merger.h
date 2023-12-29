#pragma once

//
// monotone
//
// time-series storage
//

typedef struct MergerReq MergerReq;
typedef struct Merger    Merger;

struct MergerReq
{
	Part*     origin;
	Memtable* memtable;
	Storage*  storage;

};

struct Merger
{
	PartIterator     part_iterator;
	MemtableIterator memtable_iterator;
	MergeIterator    merge_iterator;
	PartWriter       writer;
};

void merger_init(Merger*);
void merger_free(Merger*);
void merger_reset(Merger*);
Part*
merger_execute(Merger*, MergerReq*);
