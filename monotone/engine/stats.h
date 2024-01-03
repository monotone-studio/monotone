#pragma once

//
// monotone
//
// time-series storage
//

typedef struct StatsStorage StatsStorage;
typedef struct Stats        Stats;

struct StatsStorage
{
	const char* name;
	uint64_t    partitions;
	uint64_t    pending;
	uint64_t    min;
	uint64_t    max;
	uint64_t    rows;
	uint64_t    rows_cached;
	uint64_t    size;
	uint64_t    size_uncompressed;
	uint64_t    size_cached;
	uint64_t    reserved[8];
};

struct Stats
{
	uint64_t lsn;
	uint64_t rows_written;
	uint64_t rows_written_bytes;
	uint32_t storages;
};

StatsStorage* engine_stats(Engine*, Stats*);
