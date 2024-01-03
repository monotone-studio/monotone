#pragma once

//
// monotone
//
// time-series storage
//

typedef struct StatStorage StatStorage;
typedef struct Stat        Stat;

struct StatStorage
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

struct Stat
{
	// main
	uint64_t lsn;
	uint64_t rows_written;
	uint64_t rows_written_bytes;

	// storages
	uint32_t storages;
	uint64_t partitions;
	uint64_t pending;
	uint64_t min;
	uint64_t max;
	uint64_t rows;
	uint64_t rows_cached;
	uint64_t size;
	uint64_t size_uncompressed;
	uint64_t size_cached;
	uint64_t reserved[8];
};

StatStorage* engine_stats(Engine*, Stat*);
