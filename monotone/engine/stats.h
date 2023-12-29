#pragma once

//
// noire
//
// time-series storage
//

typedef struct StatPart StatPart;
typedef struct Stat     Stat;

struct StatPart
{
	uint64_t min;
	uint64_t max;
	uint64_t count;
	uint64_t size;
	uint64_t cache_count;
	uint64_t cache_size;
	uint32_t tier;
};

struct Stat
{
	uint64_t size;
	uint64_t count;
	uint32_t count_parts;
	StatPart parts[];
};

Stat* engine_stats(Engine*);
