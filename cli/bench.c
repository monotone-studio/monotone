
//
// monotone
//
// time-series storage
//

#include <monotone_private.h>
#include <monotone.h>
#include "bench.h"

static int
instance_open(Instance* self, const char* path, int id)
{
	self->env = monotone_init(NULL, NULL);
	if (self->env == NULL)
	{
		printf("error: monotone_init() failed\n");
		return -1;
	}

	monotone_execute(self->env, "set log_to_stdout to true", NULL);
	monotone_execute(self->env, "set serial to true", NULL);
	monotone_execute(self->env, "set workers to 2", NULL);
	monotone_execute(self->env, "set wal_enable to false", NULL);

	char ref_path[PATH_MAX];
	snprintf(ref_path, sizeof(ref_path), "%s/%d", path, id);

	mkdir(path, 0755);

	int rc;
	rc = monotone_open(self->env, ref_path);
	if (rc == -1)
	{
		printf("error: %s\n", monotone_error(self->env));
		return -1;
	}

	monotone_execute(self->env, "alter storage main set (compression 'zstd', refresh_wm 0)", NULL);
	return 0;
}

static void
instance_close(Instance* self)
{
	if (self->env)
	{
		monotone_free(self->env);
		self->env = NULL;
	}
}

static void*
instance_writer(void* arg)
{
	Instance* self = arg;

	const int metrics = 25;
	float     data[metrics]; // ~ 100 bytes
	for (int i = 0; i < metrics; i++)
		data[i] = 0.345;

	int              batch_size = 200;
	monotone_event_t batch[batch_size];

	while (self->writer_active)
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
		rc = monotone_write(self->env, batch, batch_size);
		if (rc == -1)
		{
			printf("error: %s\n", monotone_error(self->env));
			continue;
		}

		atomic_u64_add(&self->written, batch_size);
		atomic_u64_add(&self->written_bytes, sizeof(data) * batch_size);
	}

	return NULL;
}

void
bench_init(Bench* self)
{
	memset(self, 0, sizeof(*self));
}

static void
bench_close(Bench* self)
{
	for (int i = 0; i < self->instances_count; i++)
		instance_close(&self->instances[i]);
	if (self->instances)
		free(self->instances);
}

static int
bench_open(Bench* self, BenchArgs* args)
{
	if (args->count <= 0)
		return -1;

	self->instances_count = args->count;
	self->instances = calloc(args->count, sizeof(Instance));
	if (self->instances == NULL)
		return -1;

	for (int i = 0; i < self->instances_count; i++)
	{
		int rc = instance_open(&self->instances[i], args->path, i);
		if (rc == -1)
			return -1;
	}

	return 0;
}

static void*
bench_reporter(void* arg)
{
	Bench* self = arg;

	while (self->report_active)
	{
		sleep(1);

		uint64_t written = 0;
		uint64_t written_bytes = 0;
		for (int i = 0; i < self->instances_count; i++)
		{
			auto ref = &self->instances[i];
			written += atomic_u64_of(&ref->written);
			written_bytes += atomic_u64_of(&ref->written_bytes);
		}

		printf("\n");
		printf("write: %d rps (%.2fM) %.2f million metrics/sec\n",
			   (int)(written - self->written_last),
			   (written_bytes - self->written_last_bytes)  / 1024.0 / 1024.0,
			   (float)((int)((written_bytes - self->written_last_bytes - sizeof(uint64_t)) / sizeof(uint32_t))) / 1000000.0 );

		self->written_last = written;
		self->written_last_bytes = written_bytes;
	}

	return NULL;
}

static void
bench_start(Bench* self)
{
	if (self->active)
	{
		printf("error: already active\n");
		return;
	}

	for (int i = 0; i < self->instances_count; i++)
	{
		auto ref = &self->instances[i];
		ref->writer_active = true;
		pthread_create(&ref->writer_id, NULL, instance_writer, ref);
	}

	self->report_active = true;
	pthread_create(&self->report_id, NULL, bench_reporter, self);

	self->active = true;
}

static void
bench_stop(Bench* self)
{
	if (! self->active)
	{
		printf("error: not active\n");
		return;
	}

	for (int i = 0; i < self->instances_count; i++)
	{
		auto ref = &self->instances[i];
		ref->writer_active = false;
		pthread_join(ref->writer_id, NULL);
	}

	self->report_active = false;
	pthread_join(self->report_id, NULL);

	self->active = false;
}

static void
bench_cli(Bench* self)
{
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
			bench_start(self);
			continue;
		}

		if (! strcmp(command, "/stop"))
		{
			bench_stop(self);
			continue;
		}

		if (! strcmp(command, "/help"))
		{
			printf("/switch -- set current instance\n");
			printf("/start  -- start benchmark\n");
			printf("/stop   -- stop  benchmark\n");
			printf("/exit   -- quit\n");
			continue;
		}

		if (! strcmp(command, "/switch"))
		{
			// todo
			continue;
		}

		if (! strcmp(command, "/exit"))
			break;
	}

	if (self->active)
		bench_stop(self);
}

int
bench_main(Bench* self, BenchArgs* args)
{
	int rc = bench_open(self, args);
	if (rc == -1)
	{
		bench_close(self);
		return EXIT_FAILURE;
	}
	bench_cli(self);
	bench_close(self);
	return EXIT_SUCCESS;
}
