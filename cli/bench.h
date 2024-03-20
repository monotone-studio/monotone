#pragma once

//
// monotone
//
// time-series storage
//

typedef struct Instance    Instance;
typedef struct BenchConfig BenchConfig;
typedef struct Bench       Bench;

struct Instance
{
	volatile bool writer_active;
	pthread_t     writer_id;
	atomic_u64    written;
	atomic_u64    written_bytes;
	uint64_t      written_last;
	uint64_t      written_last_bytes;
	monotone_t*   env;
	BenchConfig*  config;
};

struct BenchConfig
{
	const char* path;
	int         instances;
	int         workers;
	int         size_event;
	int         size_metric;
	int         size_batch;
	bool        wal;
};

struct Bench
{
	bool          active;
	volatile bool report_active;
	pthread_t     report_id;
	atomic_u32    current;
	int           instances_count;
	Instance*     instances;
	BenchConfig*  config;
};

void bench_init(Bench*);
int  bench_main(Bench*, BenchConfig*);
