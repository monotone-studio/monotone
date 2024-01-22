
//
// monotone
//
// time-series storage
//

#include <monotone_private.h>
#include <monotone.h>

static monotone_t*  env;

static volatile int writer_run;
static uint64_t     writer_seq;
static pthread_t    writer_id;

static volatile int report_run;
static pthread_t    report_id;

static void*
writer_main(void* arg)
{
	const int metrics = 25;
	float data[metrics]; // ~ 100 bytes
	for (int i = 0; i < metrics; i++)
		data[i] = 0.345;

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

static inline void
report_rows(uint64_t* rows_written, uint64_t* rows_written_bytes)
{
	char* result = NULL;
	int rc = monotone_execute(env, "show rows_written", &result);
	if (rc == -1)
	{
		printf("error: %s\n", monotone_error(env));
		return;
	}
	if (result)
	{
		*rows_written = strtoull(result, NULL, 10);
		free(result);
	}
	rc = monotone_execute(env, "show rows_written_bytes", &result);
	if (rc == -1)
	{
		printf("error: %s\n", monotone_error(env));
		return;
	}
	if (result)
	{
		*rows_written_bytes = strtoull(result, NULL, 10);
		free(result);
	}
}

static void
report_print(void)
{
	uint64_t rows_written = 0;
	uint64_t rows_written_bytes = 0;
	report_rows(&rows_written, &rows_written_bytes);

	char* result = NULL;
	int rc = monotone_execute(env, "show storages", &result);
	if (rc == -1)
	{
		printf("error: %s\n", monotone_error(env));
		return;
	}
	if (result)
	{
		printf("%s\n", result);
		free(result);
	}

	/*
	uint64_t sec = rows_written_bytes - last_written_bytes;
	printf("rows                %" PRIu64 "\n", rows);
	printf("partitions          %" PRIu64 "\n", partitions);
	printf("size                %" PRIu64 " Gb\n", size / 1024 / 1024 / 1024);
	printf("  min               %" PRIu64 " Gb\n", (size + sec * 60) / 1024 / 1024 / 1024);
	printf("  hour              %" PRIu64 " Gb\n", (size + sec * 60 * 60) / 1024 / 1024 / 1024);
	printf("  day               %" PRIu64 " Tb\n", (size + sec * 60 * 60 * 24) / 1024 / 1024 / 1024 / 1024);
	printf("  week              %" PRIu64 " Tb\n", (size + sec * 60 * 60 * 24 * 7) / 1024 / 1024 / 1024 / 1024);
	printf("  month             %" PRIu64 " Tb\n", (size + sec * 60 * 60 * 24 * 7 * 31) / 1024 / 1024 / 1024 / 1024);
	printf("\n");
	*/

	printf("\n");
	printf("write: %d rps (%.2f Mbs) %.2fM metrics/sec\n",
	       (int)(rows_written - last_written),
	       (rows_written_bytes - last_written_bytes)  / 1024.0 / 1024.0,
	       (float)((int)((rows_written_bytes - last_written_bytes - sizeof(uint64_t)) / sizeof(uint32_t))) / 1000000.0 );

	last_written = rows_written;
	last_written_bytes = rows_written_bytes;
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
	uint64_t size = 0;

	monotone_row_t row;
	while (monotone_read(cursor, &row))
	{
		n++;
		size += sizeof(uint64_t) + row.data_size;

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
	double bps = size / (float)(diff / 1000.0 / 1000.0);

	printf("%f rps\n", rps);
	printf("%f mbs\n", bps / 1024 / 1024);

	printf("rows: %d\n", (int)n);

	monotone_free(cursor);
}

static void
cli(void)
{
	env = monotone_init(NULL, NULL);
	monotone_execute(env, "set log_to_stdout to true", NULL);
	monotone_execute(env, "set workers to 3", NULL);

	int rc;
	rc = monotone_open(env, "./t");
	if (rc == -1)
	{
		printf("error: %s\n", monotone_error(env));
		monotone_free(env);
		return;
	}

	rc = monotone_execute(env, "create storage if not exists hot (compression 1, sync true, refresh_wm 0)", NULL);
	if (rc == -1)
	{
		printf("error: %s\n", monotone_error(env));
		return;
	}
	rc = monotone_execute(env, "create storage if not exists cold (compression 1, sync true)", NULL);
	if (rc == -1)
	{
		printf("error: %s\n", monotone_error(env));
		return;
	}
	rc = monotone_execute(env, "alter conveyor hot (capacity 10), cold", NULL);
	/*rc = monotone_execute(env, "alter conveyor hot (capacity 10), cold", NULL);*/
	if (rc == -1)
	{
		printf("error: %s\n", monotone_error(env));
		return;
	}

	for (;;)
	{
		printf("> ");
		fflush(stdout);

		char command[512];
		char* p = fgets(command, sizeof(command), stdin);
		if (p == NULL)
			break;

		for (char* pos = p; *pos; pos++)
			if (*pos == '\n')
				*pos = 0;

		if (! strcmp(command, "/start"))
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
			continue;
		}

		if (! strcmp(command, "/stop"))
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
			continue;
		}

		if (! strcmp(command, "/scan"))
		{
			scan();
			continue;
		}

		char *result = NULL;
		rc = monotone_execute(env, command, &result);
		if (rc == -1)
		{
			printf("error: %s\n", monotone_error(env));
			continue;
		}
		if (result)
		{
			printf("%s", result);
			free(result);
		}
	}

	if (writer_run)
	{
		report_run = 0;
		writer_run = 0;
		pthread_join(writer_id, NULL);
		pthread_join(report_id, NULL);
	}

	monotone_free(env);
}

int
main(int argc, char* argv[])
{
	cli();
	return 0;
}
