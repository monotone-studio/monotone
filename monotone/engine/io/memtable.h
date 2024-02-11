#pragma once

//
// monotone
//
// time-series storage
//

typedef struct MemtablePage MemtablePage;
typedef struct Memtable     Memtable;

struct MemtablePage
{
	RbtreeNode node;
	int        rows_count;
	Row*       rows[];
};

struct Memtable
{
	Comparator* comparator;
	Rbtree      tree;
	uint64_t    count;
	int         count_pages;
	uint64_t    size;
	int         size_page;
	int         size_split;
	uint64_t    lsn_min;
	uint64_t    lsn_max;
	List        iterators;
	int         iterators_count;
};

void memtable_init(Memtable*, int, int, Comparator*);
void memtable_free(Memtable*);
void memtable_move(Memtable*, Memtable*);
void memtable_set(Memtable*, Row*);
bool memtable_seek(Memtable*, Row*, MemtablePage**, int*);
void memtable_follow(Memtable*, uint64_t);
