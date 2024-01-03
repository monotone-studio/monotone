
//
// monotone
//
// time-series storage
//

#include <monotone_private.h>
#include <time.h>

static Instance     instance;

static volatile int writer_run;
static pthread_t    writer_id;
static uint64_t     writer_seq;
static volatile int report_run;
static pthread_t    report_id;

static void*
writer_main(void* arg)
{
	runtime_init(&instance.global);
	(void)arg;

	char data[100];
	memset(data, 'x', sizeof(data));

	Exception e;
	if (try(&e))
	{
		while (writer_run)
		{
			/*instance_insert(&instance, writer_seq++, "", 0);*/
			instance_insert(&instance, writer_seq++, data, sizeof(data) );
		}
	}
	if (catch(&e))
	{ }

	return NULL;
}

static atomic_u64 last_written;
static atomic_u64 last_written_bytes;

static void
report_print(void)
{
	Exception e;
	if (try(&e))
	{
		Stat stat;
		auto storages = engine_stats(&instance.engine, &stat);

		for (int i = 0; i < stat.storages; i++)
		{
			printf("%s\n", storages[i].name);
			printf("  partitions:       %" PRIu64 "\n", storages[i].partitions);
			printf("  pending:          %" PRIu64 "\n", storages[i].pending);
			printf("  min               %" PRIu64 "\n", storages[i].min);
			printf("  max               %" PRIu64 "\n", storages[i].max);
			printf("  rows              %" PRIu64 "\n", storages[i].rows);
			printf("  rows_cached       %" PRIu64 "\n", storages[i].rows_cached);
			printf("  size              %" PRIu64 " Mb\n", storages[i].size / 1024 / 1024);
			printf("  size_uncompressed %" PRIu64 " Mb\n", storages[i].size_uncompressed / 1024 / 1024);
			printf("  size_cached       %" PRIu64 " Mb\n", storages[i].size_cached / 1024 / 1024);
		}

		printf("write: %d rps (%.2f Mbs) %d metrics/sec\n",
		       (int)(stat.rows_written - last_written),
		       (stat.rows_written_bytes - last_written_bytes)  / 1024.0 / 1024.0,
		       (int)((stat.rows_written_bytes - last_written_bytes - sizeof(uint64_t)) / sizeof(uint32_t)));

		last_written = stat.rows_written;
		last_written_bytes = stat.rows_written_bytes;

		free(storages);
	}
	if (catch(&e))
	{ }
}

static void*
report_main(void* arg)
{
	runtime_init(&instance.global);
	(void)arg;

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
	uint64_t start = gettime();

	EngineCursor cursor;
	engine_cursor_init(&cursor);
	engine_cursor_open(&cursor, &instance.engine, NULL);

	uint64_t n = 0;
	for (;;)
	{
		if (! engine_cursor_has(&cursor))
			break;

		n++;
		/*
		auto row = engine_cursor_at(&cursor);
		printf("%d ", (int)row->time);
		*/
		engine_cursor_next(&cursor);
	}

	// rps
	uint64_t diff = (gettime() - start) / 1000;
	double rps = n / (float)(diff / 1000.0 / 1000.0);
	printf("%f rps\n", rps);

	printf("rows: %d\n", (int)n);
	printf("---\n");
	report_print();

	engine_cursor_close(&cursor);
}

static void
cli(void)
{
	char config[] =
	"interval 3000000,"
	"workers 3,"
	"storage hot(compaction_wm 0, capacity 10, sync false, path './hot'),"
	"storage cold(compression 1, sync, path './cold')";
	instance_start(&instance, config);

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
			Exception e;
			if (try(&e))
			{
				engine_flush(&instance.engine);
			}
			if (catch(&e))
			{ }
		} else 
		if (! strcmp(command, "/scan\n"))
		{
			Exception e;
			if (try(&e))
			{
				scan();
			}
			if (catch(&e))
			{ }
		} else
		{
			printf("error: unknown command\n");
		}
	}

	instance_stop(&instance);
}

int
main(int argc, char* argv[])
{
	runtime_init(&instance.global);
	instance_init(&instance);

	Exception e;
	if (try(&e)) {
		cli();
	}
	if (catch(&e))
	{ }

	return 0;
}