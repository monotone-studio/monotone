#pragma once

//
// monotone
//
// time-series storage
//

typedef struct Instance  Instance;
typedef struct BenchArgs BenchArgs;
typedef struct Bench     Bench;

struct Instance
{
	volatile bool writer_active;
	pthread_t     writer_id;
	atomic_u64    written;
	atomic_u64    written_bytes;
	monotone_t*   env;
};

struct BenchArgs
{
	const char* path;
	int         count;
};

struct Bench
{
	bool          active;
	uint64_t      written_last;
	uint64_t      written_last_bytes;
	volatile bool report_active;
	pthread_t     report_id;
	int           instances_count;
	Instance*     instances;
};

void bench_init(Bench*);
int  bench_main(Bench*, BenchArgs*);
