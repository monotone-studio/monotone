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

typedef struct MemtablePage MemtablePage;
typedef struct Memtable     Memtable;

struct MemtablePage
{
	RbtreeNode node;
	int        events_count;
	Event*     events[];
};

struct Memtable
{
	Comparator* comparator;
	Rbtree      tree;
	int         size_page;
	int         size_split;
	int         count_pages;
	atomic_u64  count;
	atomic_u64  lsn_min;
	atomic_u64  lsn_max;
	List        iterators;
	int         iterators_count;
	Heap        heap;
};

void   memtable_init(Memtable*, int, int, Comparator*);
void   memtable_free(Memtable*);
void   memtable_move(Memtable*, Memtable*);
Event* memtable_set(Memtable*, Event*);
void   memtable_unset(Memtable*, Event*);
Event* memtable_max(Memtable*);
bool   memtable_seek(Memtable*, Event*, MemtablePage**, int*);
void   memtable_follow(Memtable*, uint64_t);
