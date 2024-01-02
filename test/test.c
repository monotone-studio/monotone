
//
// monotone
//
// time-series storage
//

#include <monotone_private.h>
#include <time.h>

static volatile int writer_run;
static pthread_t    writer_id;
static uint64_t     writer_seq;

static volatile int report_run;
static pthread_t    report_id;
static Instance     instance;

static atomic_u64   count_wr;
static atomic_u64   count_wr_data;

static void*
writer_main(void* arg)
{
	runtime_init(&instance.global);
	(void)arg;

	char data[100];
	memset(data, 0, sizeof(data));

	Exception e;
	if (try(&e))
	{
		while (writer_run)
		{
			instance_insert(&instance, writer_seq++, "", 0);
			/*instance_insert(&instance, writer_seq++, data, sizeof(data) );*/
			atomic_u64_add(&count_wr, 1);
			atomic_u64_add(&count_wr_data, sizeof(uint64_t) /*+ sizeof(data)*/);
		}
	}
	if (catch(&e))
	{ }

	return NULL;
}



static void
report_print(void)
{
	Exception e;
	if (try(&e))
	{
		auto stat = engine_stats(&instance.engine);
		for (int i = 0; i < stat->count_parts; i++)
		{
			printf("[%020" PRIu64 ", %020" PRIu64 "] size: %" PRIu64 ", count: %" PRIu64 ", cache: %" PRIu64 ", tier: %d\n",
			       stat->parts[i].min,
			       stat->parts[i].max,
			       stat->parts[i].size,
			       stat->parts[i].count,
			       stat->parts[i].cache_count,
			       stat->parts[i].tier);
		}

		printf("partitions: %" PRIu32 "\n", stat->count_parts);
		printf("count:      %" PRIu64 "\n", stat->count);
		printf("size:       %" PRIu64 "\n", stat->size);
		printf("\n");

		free(stat);
	}
	if (catch(&e))
	{ }
}

static void*
report_main(void* arg)
{
	runtime_init(&instance.global);
	(void)arg;

	uint64_t last_wr = atomic_u64_of(&count_wr);
	uint64_t last_wr_data = atomic_u64_of(&count_wr_data);
	while (report_run)
	{
		sleep(1);
		printf("\n");

		report_print();

		uint64_t cwr  = atomic_u64_of(&count_wr);
		uint64_t cwrd = atomic_u64_of(&count_wr_data);

		printf("write: %d rps (%.2f Mbs) %d metrics/sec\n", (int)cwr - (int)last_wr,
		       (cwrd - last_wr_data)  / 1024.0 / 1024.0,
		       (int)((cwrd - last_wr_data - sizeof(uint64_t)) / sizeof(uint32_t)));

		last_wr      = atomic_u64_of(&count_wr);
		last_wr_data = atomic_u64_of(&count_wr_data);

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
	instance.config.interval = 3000000;
	instance.config.workers  = 3;

	/*
	Str name;
	str_set_cstr(&name, "test");
	auto storage = storage_allocate(&name);
	storage_mgr_add(&instance.storage_mgr, storage);
	str_strdup(&storage->directory, "./t");
	*/

	// hot
	Str name;
	str_set_cstr(&name, "hot");
	auto storage = storage_allocate(&name);
	storage_mgr_add(&instance.storage_mgr, storage);
	storage->compaction_wm = 0;
	storage->capacity = 10;
	storage->sync = false;
	str_strdup(&storage->directory, "./hot");

	// cold
	str_set_cstr(&name, "cold");
	storage = storage_allocate(&name);
	storage_mgr_add(&instance.storage_mgr, storage);
	storage->compression = 1;
	storage->capacity = 0;
	storage->sync = true;
	str_strdup(&storage->directory, "./cold");

	/*
	// drop
	str_set_cstr(&name, "drop");
	storage = storage_allocate(&name);
	storage_mgr_add(&instance.storage_mgr, storage);
	storage->compression = 0;
	storage->capacity = 0;
	storage->drop = true;
	*/

	instance_start(&instance);

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
