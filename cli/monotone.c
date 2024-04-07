
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

#include <monotone_private.h>
#include <monotone.h>
#include "bench.h"

static int
usage(void)
{
	printf("monotone cli.\n");
	printf("\n");
	printf("usage: monotone <command> [options]\n");
	printf("\n");
	printf("commands:\n");
	printf("\n");
	printf("   bench [options] [path]\n");
	printf("     -i <instances>    -- set number of parallel instances (default: 1)\n");
	printf("     -w <workers>      -- number of workers per instance (default: 3)\n");
	printf("     -s <event_size>   -- size of event data (default: 100)\n");
	printf("     -m <metric_size>  -- size of a single metric (default: 4)\n");
	printf("     -b <batch_size>   -- how many events to include in a single write (default: 200)\n");
	printf("     -n                -- disable WAL (default: WAL is enabled by default)\n");
	printf("     -c                -- add S3 cloud (default: disabled)\n");
	printf("\n");
	printf("examples:\n");
	printf("\n");
	printf("  # default benchmark\n");
	printf("  monotone bench\n");
	printf("\n");
	printf("  # benchmark four parallel instances without wal\n");
	printf("  monotone bench -i 4 -n\n");
	return EXIT_FAILURE;
}

static int
main_bench(int argc, char* argv[])
{
	// monotone bench [options] [path]
	BenchConfig config =
	{
		.instances   = 1,
		.path        = "_benchmark",
		.workers     = 3,
		.size_event  = 100,
		.size_metric = sizeof(float),
		.size_batch  = 200,
		.cloud       = false,
		.wal         = true
	};
	int opt;
	while ((opt = getopt(argc, argv, "i:w:s:m:b:nc")) != -1)
	{
		switch (opt) {
		case 'i':
			config.instances = atoi(optarg);
			break;
		case 'w':
			config.workers = atoi(optarg);
			break;
		case 's':
			config.size_event = atoi(optarg);
			break;
		case 'm':
			config.size_metric = atoi(optarg);
			break;
		case 'b':
			config.size_batch = atoi(optarg);
			break;
		case 'n':
			config.wal = false;
			break;
		case 'c':
			config.cloud = true;
			break;
		default:
			return usage();
		}
	}
	if (optind < argc)
		config.path = argv[optind];

	if (config.instances == 0)
	{
		printf("error: instances cannot be zero\n");
		return -1;
	}
	if (config.size_metric == 0)
	{
		printf("error: size_metric cannot be zero\n");
		return -1;
	}
	if (config.size_batch == 0)
	{
		printf("error: size_batch cannot be zero\n");
		return -1;
	}

	Bench bench;
	bench_init(&bench);
	return bench_main(&bench, &config);
}

int
main(int argc, char* argv[])
{
	// monotone
	if (argc == 1)
		return usage();

	// monotone bench|benchmark [options] [path]
	if (!strcmp(argv[1], "bench") ||
	    !strcmp(argv[1], "benchmark"))
		return main_bench(argc - 1, argv + 1);

	return usage();
}
