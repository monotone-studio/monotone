
//
// monotone
//
// time-series storage
//

#include <monotone_private.h>
#include <monotone.h>
#include <time.h>

static volatile int writer_run;
static uint64_t     writer_seq;
static pthread_t    writer_id;

static volatile int report_run;
static pthread_t    report_id;

monotone_t*         env;

static void*
writer_main(void* arg)
{
	char data[100];
	memset(data, 'x', sizeof(data));

	while (writer_run)
	{
		monotone_row_t row =
		{
			.time = writer_seq++,
			.data = data,
			.data_size = sizeof(data)
		};

		int rc;
		rc = monotone_insert(env, &row);
		if (rc == -1) {
			// todo
		}
	}

	return NULL;
}

static atomic_u64 last_written;
static atomic_u64 last_written_bytes;

static void
report_print(void)
{
	monotone_stats_t stats;
	auto storages = monotone_stats(env, &stats);

	uint64_t rows = 0;
	uint64_t partitions = 0;
	uint64_t size = 0;
	for (int i = 0; i < stats.storages; i++)
	{
		printf("%s\n", storages[i].name);
		printf("  partitions        %" PRIu64 "\n", storages[i].partitions);
		printf("  pending           %" PRIu64 "\n", storages[i].pending);
		printf("  min               %" PRIu64 "\n", storages[i].min);
		printf("  max               %" PRIu64 "\n", storages[i].max);
		printf("  rows              %" PRIu64 "\n", storages[i].rows);
		printf("  rows_cached       %" PRIu64 "\n", storages[i].rows_cached);
		printf("  size              %" PRIu64 " Mb\n", storages[i].size / 1024 / 1024);
		printf("  size_uncompressed %" PRIu64 " Mb\n", storages[i].size_uncompressed / 1024 / 1024);
		printf("  size_cached       %" PRIu64 " Mb\n", storages[i].size_cached / 1024 / 1024);
		rows += storages[i].rows;
		partitions += storages[i].partitions;
		size += storages[i].size;
	}

	printf("\n");
	printf("rows                %" PRIu64 "\n", rows);
	printf("partitions          %" PRIu64 "\n", partitions);
	printf("size                %" PRIu64 " Mb\n", size / 1024 / 1024);
	printf("\n");

	printf("write: %d rps (%.2f Mbs) %d metrics/sec\n",
	       (int)(stats.rows_written - last_written),
	       (stats.rows_written_bytes - last_written_bytes)  / 1024.0 / 1024.0,
	       (int)((stats.rows_written_bytes - last_written_bytes - sizeof(uint64_t)) / sizeof(uint32_t)));

	last_written = stats.rows_written;
	last_written_bytes = stats.rows_written_bytes;

	free(storages);
}

static void*
report_main(void* arg)
{
	while (report_run)
	{
		sleep(1);
		report_print();
		printf("\n");
		fflush(stdout);
	}

	return NULL;
}

uint64_t
gettime(void)
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec*  (uint64_t) 1e9 + t.tv_nsec;
}

static void
scan(void)
{
	auto cursor = monotone_cursor(env, NULL);

	uint64_t start = gettime();

	uint64_t n = 0;
	monotone_row_t row;
	while (monotone_read(cursor, &row))
	{
		n++;
		int rc = monotone_next(cursor);
		if (rc == -1)
		{
			printf("error: %s\n", monotone_error(env));
			break;
		}
	}

	// rps
	uint64_t diff = (gettime() - start) / 1000;
	double rps = n / (float)(diff / 1000.0 / 1000.0);
	printf("%f rps\n", rps);

	printf("rows: %d\n", (int)n);

	monotone_free(cursor);
}

static void
cli(void)
{
	char config[] =
	"interval 3000000,"
	"workers 3,"
	"storage hot(compaction_wm 0, capacity 10, sync false, path './hot'),"
	"storage cold(compression 1, sync, path './cold')";

	env = monotone_init(NULL, NULL);
	int rc;
	rc = monotone_open(env, config);
	if (rc == -1)
	{
		printf("error: %s\n", monotone_error(env));
		monotone_free(env);
		return;
	}

	for (;;)
	{
		printf("> ");
		fflush(stdout);

		char command[64];
		char* p = fgets(command, sizeof(command), stdin);
		if (p == NULL)
			break;

		if (! strcmp(command, "/start\n"))
		{
			if (writer_run)
			{
				printf("error: already active\n");
				continue;
			}

			writer_run = 1;
			pthread_create(&writer_id, NULL, writer_main, NULL); 

			report_run = 1;
			pthread_create(&report_id, NULL, report_main, NULL);

		} else
		if (! strcmp(command, "/stop\n"))
		{
			if (! writer_run)
			{
				printf("error: not active\n");
				continue;
			}

			report_run = 0;
			writer_run = 0;
			pthread_join(writer_id, NULL);
			pthread_join(report_id, NULL);
		} else
		if (! strcmp(command, "/checkpoint\n"))
		{
			monotone_checkpoint(env, 0);
		} else 
		if (! strcmp(command, "/scan\n"))
		{
			scan();
		} else
		{
			printf("error: unknown command\n");
		}
	}

	monotone_free(env);
}

int
main(int argc, char* argv[])
{
	cli();
	return 0;
}
