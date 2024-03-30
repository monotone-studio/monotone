
//
// monotone
//
// time-series storage
//

#include <monotone_private.h>
#include <monotone.h>
#include "bench.h"

static int
instance_open(Instance* self, BenchConfig* config, int id)
{
	self->config = config;
	self->env = monotone_init();
	if (self->env == NULL)
	{
		printf("error: monotone_init() failed\n");
		return -1;
	}

	// log to console
	monotone_execute(self->env, "set log_to_stdout to true", NULL);

	// serial
	monotone_execute(self->env, "set serial to true", NULL);

	// workers
	char sz[PATH_MAX];
	snprintf(sz, sizeof(sz), "set workers to %d", config->workers);
	monotone_execute(self->env, sz, NULL);

	// wal
	snprintf(sz, sizeof(sz), "set wal_enable to %s", config->wal ? "true": "false");
	monotone_execute(self->env, sz, NULL);

	mkdir(config->path, 0755);

	snprintf(sz, sizeof(sz), "%s/%d", config->path, id);
	int rc;
	rc = monotone_open(self->env, sz);
	if (rc == -1)
	{
		printf("error: %s\n", monotone_error(self->env));
		return -1;
	}

	if (config->cloud)
	{
		monotone_execute(self->env, "create cloud if not exists s3 (type 's3', access_key 'minioadmin', secret_key 'minioadmin', url 'localhost:9000', debug false)", NULL);
		monotone_execute(self->env, "alter storage main set (cloud 's3', compression 'zstd', refresh_wm 0)", NULL);
	} else {
		monotone_execute(self->env, "alter storage main set (compression 'zstd', refresh_wm 0)", NULL);
	}
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
	BenchConfig* config = self->config;

	uint8_t data[config->size_event];
	memset(data, 0, sizeof(data));

	float* metrics = (float*)data;
	for (int i = 0; i < config->size_event / config->size_metric; i++)
		metrics[i] = i + 0.345;

	monotone_event_t batch[config->size_batch];

	while (self->writer_active)
	{
		for (int i = 0; i < config->size_batch; i++)
		{
			auto ev = &batch[i];
			ev->id        = UINT64_MAX;
			ev->data      = data;
			ev->data_size = sizeof(data);
			ev->flags     = 0;
		}

		int rc;
		rc = monotone_write(self->env, batch, config->size_batch);
		if (rc == -1)
		{
			printf("error: %s\n", monotone_error(self->env));
			continue;
		}

		atomic_u64_add(&self->written, config->size_batch);
		atomic_u64_add(&self->written_bytes, config->size_batch * (sizeof(Event) + sizeof(data)));
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
bench_open(Bench* self, BenchConfig* config)
{
	self->config = config;
	if (config->instances <= 0)
		return -1;

	self->instances_count = config->instances;
	self->instances = calloc(self->instances_count, sizeof(Instance));
	if (self->instances == NULL)
		return -1;

	for (int i = 0; i < self->instances_count; i++)
	{
		auto ref = &self->instances[i];
		int rc = instance_open(ref, config, i);
		if (rc == -1)
			return -1;
	}

	return 0;
}

static void*
bench_reporter(void* arg)
{
	Bench* self = arg;

	uint64_t written[self->instances_count];
	uint64_t written_bytes[self->instances_count];
	uint64_t written_last[self->instances_count];
	uint64_t written_last_bytes[self->instances_count];

	while (self->report_active)
	{
		sleep(1);

		uint64_t total = 0;
		uint64_t total_bytes = 0;
		uint64_t total_last = 0;
		uint64_t total_last_bytes = 0;

		for (int i = 0; i < self->instances_count; i++)
		{
			auto ref = &self->instances[i];

			written[i]            = atomic_u64_of(&ref->written);
			written_bytes[i]      = atomic_u64_of(&ref->written_bytes);
			written_last[i]       = ref->written_last;
			written_last_bytes[i] = ref->written_last_bytes;

			total            += written[i];
			total_bytes      += written_bytes[i];
			total_last       += written_last[i];
			total_last_bytes += written_last_bytes[i];

			ref->written_last = written[i];
			ref->written_last_bytes = written_bytes[i];
		}

		printf("\n");

		for (int i = 0; i < self->instances_count; i++)
		{
			auto ref = &self->instances[i];
			char* result = NULL;
			int rc = monotone_execute(ref->env, "show storages", &result);
			if (rc == -1)
			{
				printf("error: %s\n", monotone_error(ref->env));
			}
			if (result)
			{
				printf("%s\n", result);
				free(result);
			}
		}

		uint64_t rps = total - total_last;
		double eps = (double)((total - total_last) / 1000000.0);
		double mps = (double)(((total_bytes - total_last_bytes) / (double)self->config->size_metric)) / 1000000.0;
		double dps = (double)((total_bytes - total_last_bytes) / 1024.0 / 1024.0);

		if (self->config->size_event == 0)
			mps = 0;

		printf("write: %" PRIu64 " rps (%.2f million events/sec, %.2f million metrics/sec), %.2f MiB/sec\n",
		       rps, eps, mps, dps);
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
bench_select(Bench* self, uint64_t from, uint64_t count)
{
	auto current = &self->instances[atomic_u32_of(&self->current)];

	uint64_t time = time_us();
	uint64_t total = 0;
	uint64_t total_size = 0;

	monotone_event_t key;
	key.id        = from;
	key.data      = NULL;
	key.data_size = 0;
	key.flags     = 0;

	auto cursor = monotone_cursor(current->env, NULL, &key);
	if (cursor == NULL)
	{
		printf("error: %s\n", monotone_error(current->env));
		return;
	}

	monotone_event_t event;
	while (total < count)
	{
		int rc = monotone_read(cursor, &event);
		if (rc == -1)
		{
			printf("error: %s\n", monotone_error(current->env));
			break;
		}
		if (rc == 0)
			break;

		total++;
		total_size += sizeof(Event) + event.data_size;

		rc = monotone_next(cursor);
		if (rc == -1)
		{
			printf("error: %s\n", monotone_error(current->env));
			break;
		}
	}

	monotone_free(cursor);

	double   diff = (time_us() - time) / 1000.0 / 1000.0;
	double   rps  = total / diff;
	double   eps  = (double)rps / 1000000.0;
	double   mps  = (((double)total_size / (double)self->config->size_metric) / 1000000.0) / diff;
	if (self->config->size_event == 0)
		mps = 0;
	double   dps  = (double)(total_size / 1024 / 1024) / diff;

	printf("read:         %.0f rps (%.2f million events/sec, %.2f million metrics/sec), %.2f MiB/sec\n",
	       rps, eps, mps, dps);
	printf("read events:  %" PRIu64 " (%.1f millions)\n", total, (double)total / 1000000.0);
	printf("read metrics: %.1f millions\n",
	       (double)(total_size - (sizeof(Event) * total) / self->config->size_metric) / 1000.0 / 1000.0);
	printf("read size:    %.1f MiB\n",
	       (double)total_size / 1024.0 / 1024.0);
	printf("read time:    %.1f secs\n", diff);
}

static void
bench_help(Bench* self)
{
	unused(self);
	printf("\nmonotone benchmarking.\n\n");
	printf("/switch <instance>       -- set current instance\n");
	printf("/start                   -- start benchmark\n");
	printf("/stop                    -- stop  benchmark\n");
	printf("/select <start> <count>  -- read events using current instance\n");
	printf("/help                    -- show help\n\n");
	printf("Other commands will be executed using current instance.");
	printf("\n");
}

static void
bench_cli(Bench* self)
{
	bench_help(self);

	for (;;)
	{
		if (self->instances_count == 1)
			printf("> ");
		else
			printf("instance %d> ", atomic_u32_of(&self->current));
		fflush(stdout);

		char command[512];
		char* p = fgets(command, sizeof(command), stdin);
		if (p == NULL)
			break;

		for (char* pos = command; *pos; pos++)
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
			bench_help(self);
			continue;
		}

		if (! strncmp(command, "/switch", 7))
		{
			char* ptr = command + 7;
			int order = 0;
			if (sscanf(ptr, "%d", &order) != 1)
			{
				printf("error: please provide instance number\n");
				continue;
			}
			if (order < 0 || order >= self->instances_count)
			{
				printf("error: instance order is out of range\n");
				continue;
			}
			atomic_u32_set(&self->current, order);
			continue;
		}

		if (! strncmp(command, "/select", 7))
		{
			char*    ptr   = command + 7;
			uint64_t start = 0;
			uint64_t count = 0;
			if (sscanf(ptr, "%"SCNu64"%"SCNu64, &start, &count) != 2)
			{
				printf("error: /select <start> <count> expected\n");
				continue;
			}
			bench_select(self, start, count);
			continue;
		}

		if (! strcmp(command, "/exit"))
			break;

		auto current = &self->instances[atomic_u32_of(&self->current)];
		char* result = NULL;
		int rc = monotone_execute(current->env, command, &result);
		if (rc == -1)
		{
			printf("error: %s\n", monotone_error(current->env));
			continue;
		}
		if (result)
		{
			printf("%s\n", result);
			free(result);
		}
	}

	if (self->active)
		bench_stop(self);
}

int
bench_main(Bench* self, BenchConfig* config)
{
	int rc = bench_open(self, config);
	if (rc == -1)
	{
		bench_close(self);
		return EXIT_FAILURE;
	}
	bench_cli(self);
	bench_close(self);
	return EXIT_SUCCESS;
}
