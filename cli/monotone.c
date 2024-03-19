
//
// monotone
//
// time-series storage
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
	printf("   bench [-t] <path>\n");
	printf("\n");
	return EXIT_FAILURE;
}

static int
main_bench(int argc, char* argv[])
{
	// monotone bench [-i instances] <path>
	int count = 1;
	int opt;
	while ((opt = getopt(argc, argv, "i:")) != -1)
	{
		switch (opt) {
		case 'i':
			count = atoi(optarg);
			break;
		default:
			return usage();
		}
	}

	if (optind >= argc)
	{
		printf("bench: path is not specified\n");
		return EXIT_FAILURE;
	}

	BenchArgs args =
	{
		.count = count,
		.path  = argv[optind]
	};
	Bench bench;
	bench_init(&bench);
	return bench_main(&bench, &args);
}

int
main(int argc, char* argv[])
{
	// monotone
	if (argc == 1)
		return usage();

	// monotone bench|benchmark [-t] <path>
	if (!strcmp(argv[1], "bench") ||
	    !strcmp(argv[1], "benchmark"))
		return main_bench(argc - 1, argv + 1);

	return usage();
}
