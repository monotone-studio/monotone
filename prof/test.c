
//
// monotone
//
// time-series storage
//

#include <monotone_private.h>
#include <monotone.h>

static monotone_t*  env;

static volatile int writer_run;
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

	int              batch_size = 200;
	monotone_event_t batch[batch_size];

	while (writer_run)
	{
		for (int i = 0; i < batch_size; i++)
		{
			auto ev = &batch[i];
			ev->id        = UINT64_MAX;
			ev->data      = data;
			ev->data_size = sizeof(data);
			ev->remove    = false;
		}

		int rc;
		rc = monotone_write(env, batch, batch_size);
		if (rc == -1) {
			// todo
		}
	}

	return NULL;
}

static atomic_u64 last_written;
static atomic_u64 last_written_bytes;

static inline void
report_events(uint64_t* events_written, uint64_t* events_written_bytes)
{
	char* result = NULL;
	int rc = monotone_execute(env, "show events_written", &result);
	if (rc == -1)
	{
		printf("error: %s\n", monotone_error(env));
		return;
	}
	if (result)
	{
		*events_written = strtoull(result, NULL, 10);
		free(result);
	}
	rc = monotone_execute(env, "show events_written_bytes", &result);
	if (rc == -1)
	{
		printf("error: %s\n", monotone_error(env));
		return;
	}
	if (result)
	{
		*events_written_bytes = strtoull(result, NULL, 10);
		free(result);
	}
}

static void
report_print(void)
{
	uint64_t events_written = 0;
	uint64_t events_written_bytes = 0;
	report_events(&events_written, &events_written_bytes);

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
	uint64_t sec = events_written_bytes - last_written_bytes;
	printf("events                %" PRIu64 "\n", events);
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
	printf("write: %d rps (%.2fM) %.2f million metrics/sec\n",
	       (int)(events_written - last_written),
	       (events_written_bytes - last_written_bytes)  / 1024.0 / 1024.0,
	       (float)((int)((events_written_bytes - last_written_bytes - sizeof(uint64_t)) / sizeof(uint32_t))) / 1000000.0 );

	last_written = events_written;
	last_written_bytes = events_written_bytes;
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
	auto cursor = monotone_cursor(env, NULL, NULL);

	uint64_t start = gettime();

	uint64_t n = 0;
	uint64_t size = 0;

	monotone_event_t event;
	while (monotone_read(cursor, &event))
	{
		n++;
		size += sizeof(uint64_t) + event.data_size;

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

	printf("events: %d\n", (int)n);

	monotone_free(cursor);
}

static void
cli(void)
{
	env = monotone_init(NULL, NULL);
	monotone_execute(env, "set log_to_stdout to true", NULL);
	monotone_execute(env, "set serial to true", NULL);
	monotone_execute(env, "set workers to 3", NULL);
	/*monotone_execute(env, "set wal_enable to false", NULL);*/

	int rc;
	rc = monotone_open(env, "./t");
	if (rc == -1)
	{
		printf("error: %s\n", monotone_error(env));
		monotone_free(env);
		return;
	}

	/*rc = monotone_execute(env, "alter storage main set (compression 'zstd', refresh_wm 0)", NULL);*/

	rc = monotone_execute(env, "create cloud if not exists s3 (type 's3', access_key 'minioadmin', secret_key 'minioadmin', url 'localhost:9000', debug false)", NULL);
	if (rc == -1)
	{
		printf("error: %s\n", monotone_error(env));
		return;
	}

	rc = monotone_execute(env, "create storage if not exists test (cloud 's3', compression 'zstd', refresh_wm 0, region_size 512KiB)", NULL);
	if (rc == -1)
	{
		printf("error: %s\n", monotone_error(env));
		return;
	}

	rc = monotone_execute(env, "alter pipeline set test", NULL);
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
			printf("%s\n", result);
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
