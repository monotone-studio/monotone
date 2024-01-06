
//
//
// time-series storage
//

#include <monotone_private.h>
#include <monotone.h>
#include <monotone_test.h>
#include <dlfcn.h>

static TestCursor*
test_cursor_new(TestSuite* self, const char* name)
{
	TestCursor* cursor = malloc(sizeof(*cursor));
	if (cursor == NULL)
		return NULL;
	cursor->name = strdup(name);
	if (cursor->name == NULL) {
		free(cursor);
		return NULL;
	}
	cursor->cursor = NULL;
	list_init(&cursor->link);
	list_append(&self->list_cursor, &cursor->link);
	self->list_cursor_count++;
	return cursor;
}

static TestCursor*
test_cursor_find(TestSuite* self, const char* name)
{
	list_foreach(&self->list_cursor)
	{
		auto cursor = list_at(TestCursor, link);
		if (strcasecmp(name, cursor->name) == 0)
			return cursor;
	}
	return NULL;
}

static void
test_cursor_free(TestSuite* self, TestCursor* cursor)
{
	list_unlink(&cursor->link);
	self->list_cursor_count--;
	if (cursor->cursor)
		monotone_free(cursor->cursor);
	free(cursor->name);
	free(cursor);
}

static int
test_suite_cmd_trap(TestSuite* self, char* arg)
{
	unused(self);
	unused(arg);

	kill(getpid(), SIGTRAP);
	return 0;
}

static int
test_suite_cmd_unit(TestSuite* self, char* arg)
{
	char* name = test_arg(&arg);
	if (name == NULL)
	{
		test_error(self, "line %d: unit <name> expected",
		           self->current_line);
		return -1;
	}

	void* ptr = dlsym(self->dlhandle, name);
	if (ptr == NULL)
	{
		test_error(self, "unit: test '%s' function not found\n", name);
		return -1;
	}
	void (*test_function)(void) = ptr;
	test_function();
	return 0;
}

static int
test_suite_cmd_version(TestSuite* self, char* arg)
{
	test_log(self, "%d\n", monotone_version());
	return 0;
}

static int
test_suite_cmd_open(TestSuite* self, char* arg)
{
	char* config = arg;
	if (self->env)
	{
		test_error(self, "line %d: open: env already openned",
		           self->current_line);
		return -1;
	}

	monotone_t* env = monotone_init(NULL, NULL);
	if (env == NULL) {
		test_error(self, "line %d: open: monotone_init() failed",
		           self->current_line);
		return -1;
	}
	char prefmt_config[1024];
	snprintf(prefmt_config, sizeof(prefmt_config),
	         "path '%s', workers 0, %s%s %s",
	         self->option_result_dir,
	         (self->current_test_options) ? self->current_test_options : "",
	         (self->current_test_options) ? "," : "",
	         (config) ? config : "");

	int rc;
	rc = monotone_open(env, prefmt_config);
	if (rc == -1)
	{
		test_log(self, "error: %s\n", monotone_error(self->env));
		monotone_free(env);
		return 0;
	}
	self->env = env;
	return 0;
}

static int
test_suite_cmd_close(TestSuite* self, char* arg)
{
	if (! self->env)
	{
		test_log(self, "close: env is not openned\n");
		return 0;
	}
	if (self->list_cursor_count > 0) {
		test_error(self, "line %d: close: env has openned cursors left",
		           self->current_line);
		return -1;
	}
	monotone_free(self->env);
	self->env = NULL;
	return 0;
}

static int
test_suite_cmd_error(TestSuite* self, char* arg)
{
	test_log_error(self);
	return 0;
}

static int
test_suite_cmd_stats(TestSuite* self, char* arg)
{
	if (! self->env)
	{
		test_log(self, "error: env is not openned\n");
		return 0;
	}

	monotone_stats_t stats;
	auto storages = monotone_stats(self->env, &stats);
	if (unlikely(storages == NULL))
	{
		test_log_error(self);
		return 0;
	}

	test_log(self, "stats\n");
	test_log(self, "  lsn                %" PRIu64 "\n", stats.lsn);
	test_log(self, "  rows_written       %" PRIu64 "\n", stats.rows_written);
	test_log(self, "  rows_written_bytes %" PRIu64 "\n", stats.rows_written_bytes);
	test_log(self, "  storages           %" PRIu32 "\n", stats.storages);

	for (int i = 0; i < stats.storages; i++)
	{
		test_log(self, "%s\n", storages[i].name);
		test_log(self, "  partitions         %" PRIu64 "\n", storages[i].partitions);
		test_log(self, "  pending            %" PRIu64 "\n", storages[i].pending);
		test_log(self, "  min                %" PRIu64 "\n", storages[i].min);
		test_log(self, "  max                %" PRIu64 "\n", storages[i].max);
		test_log(self, "  rows               %" PRIu64 "\n", storages[i].rows);
		test_log(self, "  rows_cached        %" PRIu64 "\n", storages[i].rows_cached);
		test_log(self, "  size               %" PRIu64 "\n", storages[i].size);
		test_log(self, "  size_uncompressed  %" PRIu64 "\n", storages[i].size_uncompressed);
		test_log(self, "  size_cached        %" PRIu64 "\n", storages[i].size_cached);
	}

	free(storages);
	return 0;
}

static int
test_suite_cmd_insert(TestSuite* self, char* arg)
{
	char* arg_time = test_arg(&arg);
	char* arg_value = test_arg(&arg);

	if (arg_time == NULL)
	{
		test_error(self, "line %d: insert <time> [value] expected",
		           self->current_line);
		return -1;
	}

	if (! self->env)
	{
		test_log(self, "error: env is not openned\n");
		return 0;
	}

	monotone_row_t row =
	{
		.time = strtoull(arg_time, NULL, 10),
		.data = arg_value,
		.data_size = arg_value ? strlen(arg_value) : 0
	};
	int rc = monotone_insert(self->env, &row);
	if (rc == -1)
		test_log_error(self);

	return 0;
}

static int
test_suite_cmd_delete(TestSuite* self, char* arg)
{
	char* arg_time = test_arg(&arg);
	char* arg_value = test_arg(&arg);

	if (arg_time == NULL)
	{
		test_error(self, "line %d: insert <time> [value] expected",
		           self->current_line);
		return -1;
	}

	if (! self->env)
	{
		test_log(self, "error: env is not openned\n");
		return 0;
	}

	monotone_row_t row =
	{
		.time = strtoull(arg_time, NULL, 10),
		.data = arg_value,
		.data_size = arg_value ? strlen(arg_value) : 0
	};
	int rc = monotone_delete(self->env, &row);
	if (rc == -1)
		test_log_error(self);

	return 0;
}

static int
test_suite_cmd_delete_by(TestSuite* self, char* arg)
{
	char* arg_name = test_arg(&arg);

	if (arg_name == NULL)
	{
		test_error(self, "line %d: delete_by <cursor> expected",
		           self->current_line);
		return -1;
	}

	if (! self->env)
	{
		test_log(self, "error: env is not openned\n");
		return 0;
	}

	auto cursor = test_cursor_find(self, arg_name);
	if (! cursor)
	{
		test_error(self, "line %d: plan: cursor not found",
		           self->current_plan_line);
		return -1;
	}

	if (cursor->cursor == NULL)
		return -1;

	int rc = monotone_delete_by(cursor->cursor);
	if (rc == -1)
		test_log_error(self);

	return 0;
}

static int
test_suite_cmd_update_by(TestSuite* self, char* arg)
{
	char* arg_name  = test_arg(&arg);
	char* arg_time  = test_arg(&arg);
	char* arg_value = test_arg(&arg);

	if (arg_name == NULL)
	{
		test_error(self, "line %d: delete_by <cursor> <time> [value] expected",
		           self->current_line);
		return -1;
	}

	if (arg_time == NULL)
	{
		test_error(self, "line %d: delete_by <cursor> <time> [value] expected",
		           self->current_line);
		return -1;
	}

	if (! self->env)
	{
		test_log(self, "error: env is not openned\n");
		return 0;
	}

	auto cursor = test_cursor_find(self, arg_name);
	if (! cursor)
	{
		test_error(self, "line %d: plan: cursor not found",
		           self->current_plan_line);
		return -1;
	}

	monotone_row_t row =
	{
		.time = strtoull(arg_time, NULL, 10),
		.data = arg_value,
		.data_size = arg_value ? strlen(arg_value) : 0
	};
	int rc = monotone_update_by(cursor->cursor, &row);
	if (rc == -1)
		test_log_error(self);

	return 0;
}

static int
test_suite_cmd_cursor(TestSuite* self, char* arg)
{
	char* arg_name  = test_arg(&arg);
	char* arg_time  = test_arg(&arg);
	char* arg_value = test_arg(&arg);

	if (arg_name == NULL)
	{
		test_error(self, "line %d: cursor <name> <time> [value] expected",
		           self->current_line);
		return -1;
	}

	if (! self->env)
	{
		test_log(self, "error: env is not openned\n");
		return 0;
	}

	auto cursor = test_cursor_find(self, arg_name);
	if (cursor)
	{
		test_error(self, "line %d: plan: cursor redefined",
		           self->current_plan_line);
		return -1;
	}

	cursor = test_cursor_new(self, arg_name);

	monotone_row_t* key = NULL;
	monotone_row_t  row;
	if (arg_time)
	{
		row.time = strtoull(arg_time, NULL, 10);
		row.data = arg_value;
		row.data_size = arg_value ? strlen(arg_value) : 0;
		key = &row;
	}

	cursor->cursor = monotone_cursor(self->env, key);
	if (cursor->cursor == NULL)
		test_log_error(self);

	return 0;
}

static int
test_suite_cmd_cursor_close(TestSuite* self, char* arg)
{
	char* arg_name = test_arg(&arg);

	if (arg_name == NULL)
	{
		test_error(self, "line %d: cursor_close <name> expected",
		           self->current_line);
		return -1;
	}

	if (! self->env)
	{
		test_log(self, "error: env is not openned\n");
		return 0;
	}

	auto cursor = test_cursor_find(self, arg_name);
	if (! cursor)
	{
		test_error(self, "line %d: plan: cursor not found",
		           self->current_plan_line);
		return -1;
	}

	test_cursor_free(self, cursor);
	return 0;
}

static int
test_suite_cmd_read(TestSuite* self, char* arg)
{
	char* arg_name = test_arg(&arg);

	if (arg_name == NULL)
	{
		test_error(self, "line %d: read <cursor> expected",
		           self->current_line);
		return -1;
	}

	if (! self->env)
	{
		test_log(self, "error: env is not openned\n");
		return 0;
	}

	auto cursor = test_cursor_find(self, arg_name);
	if (! cursor)
	{
		test_error(self, "line %d: plan: cursor not found",
		           self->current_plan_line);
		return -1;
	}

	if (cursor->cursor == NULL)
		return -1;

	monotone_row_t row;
	int rc = monotone_read(cursor->cursor, &row);
	if (rc == 0)
	{
		test_log(self, "(eof)\n");
	} else
	{
		if (row.data_size > 0)
			test_log(self, "[%" PRIu64 ", %.*s]\n", row.time, row.data_size, row.data);
		else
			test_log(self, "[%" PRIu64 "]\n", row.time);
	}
	return 0;
}

static int
test_suite_cmd_next(TestSuite* self, char* arg)
{
	char* arg_name = test_arg(&arg);

	if (arg_name == NULL)
	{
		test_error(self, "line %d: next <cursor> expected",
		           self->current_line);
		return -1;
	}

	if (! self->env)
	{
		test_log(self, "error: env is not openned\n");
		return 0;
	}

	auto cursor = test_cursor_find(self, arg_name);
	if (! cursor)
	{
		test_error(self, "line %d: plan: cursor not found",
		           self->current_plan_line);
		return -1;
	}

	if (cursor->cursor == NULL)
		return -1;

	int rc = monotone_next(cursor->cursor);
	if (rc == -1)
	{
		test_log_error(self);
	} else
	if (rc == 0)
	{
		test_log(self, "(eof)\n");
	}
	return 0;
}

static int
test_suite_cmd_select(TestSuite* self, char* arg)
{
	char* arg_time  = test_arg(&arg);
	char* arg_value = test_arg(&arg);

	if (! self->env)
	{
		test_log(self, "error: env is not openned\n");
		return 0;
	}

	monotone_row_t* key = NULL;
	monotone_row_t  row;
	if (arg_time)
	{
		row.time = strtoull(arg_time, NULL, 10);
		row.data = arg_value;
		row.data_size = arg_value ? strlen(arg_value) : 0;
		key = &row;
	}

	auto cursor = monotone_cursor(self->env, key);
	if (cursor == NULL)
	{
		test_log_error(self);
		return 0;
	}

	while (monotone_read(cursor, &row))
	{
		if (row.data_size > 0)
			test_log(self, "[%" PRIu64 ", %.*s]\n", row.time, row.data_size, row.data);
		else
			test_log(self, "[%" PRIu64 "]\n", row.time);

		int rc = monotone_next(cursor);
		if (rc == -1)
		{
			test_log_error(self);
			break;
		}
	}

	test_log(self, "(eof)\n");

	monotone_free(cursor);
	return 0;
}

static int
test_suite_cmd_checkpoint(TestSuite* self, char* arg)
{
	if (! self->env)
	{
		test_log(self, "error: env is not openned\n");
		return 0;
	}

	int rc = monotone_checkpoint(self->env);
	if (rc == -1)
		test_log_error(self);
	return 0;
}

static int
test_suite_cmd_drop(TestSuite* self, char* arg)
{
	char* arg_min = test_arg(&arg);
	char* arg_max = test_arg(&arg);

	if (arg_min == NULL || arg_max == NULL)
	{
		test_error(self, "line %d: drop <min> <max> expected",
		           self->current_line);
		return -1;
	}

	if (! self->env)
	{
		test_log(self, "error: env is not openned\n");
		return 0;
	}

	int rc = monotone_drop(self->env, strtoull(arg_min, NULL, 10),
	                       strtoull(arg_max, NULL, 10));
	if (rc == -1)
		test_log_error(self);

	return 0;
}

static int
test_suite_cmd_files(TestSuite* self, char* arg)
{
	char* arg_dir = test_arg(&arg);

	char path[PATH_MAX];
	snprintf(path, sizeof(path), "%s/%s", self->option_result_dir, arg_dir);

	DIR* dir = opendir(path);
	if (unlikely(dir == NULL))
	{
		test_error(self, "failed to open directory");
		return -1;
	}

	for (;;)
	{
		auto entry = readdir(dir);
		if (entry == NULL)
			break;
		if (entry->d_name[0] == '.')
			continue;
		test_log(self, "%s\n", entry->d_name);
	}

	closedir(dir);
	return 0;
}

static int
test_suite_cmd_partitions(TestSuite* self, char* arg)
{
	auto engine = &self->env->instance.engine;
	list_foreach(&engine->list)
	{
		auto part = list_at(Part, link);
		test_log(self, "[%" PRIu64 ", %" PRIu64 "] tier: %d\n",
		         part->min, part->max,
		         part->storage->order);
	}
	return 0;
}

static inline Part*
part_find(Engine* self, uint64_t min)
{
	list_foreach(&self->list)
	{
		auto part = list_at(Part, link);
		if (part->min == min)
			return part;
	}
	return NULL;
}

static int
test_suite_cmd_partition(TestSuite* self, char* arg)
{
	char* arg_min = test_arg(&arg);

	if (arg_min == NULL)
	{
		test_error(self, "line %d: partition <id> expected",
		           self->current_line);
		return -1;
	}
	uint64_t id = strtoull(arg_min, NULL, 10);

	auto engine = &self->env->instance.engine;
	auto part = part_find(engine, id);
	if (part == NULL)
	{
		test_log(self, "partition %" PRIu64 " not found\n", id);
		return 0;
	}

	test_log(self, "service  %d\n", part->service);
	test_log(self, "min      %" PRIu64 "\n", part->min);
	test_log(self, "max      %" PRIu64 "\n", part->max);
	test_log(self, "storage  %s\n", str_of(&part->storage->name));
	test_log(self, "memtable_a\n");
	test_log(self, "  count  %" PRIu64 "\n", part->memtable_a.count);
	test_log(self, "  size   %" PRIu64 "\n", part->memtable_a.size);
	test_log(self, "memtable_b\n");
	test_log(self, "  count  %" PRIu64 "\n", part->memtable_b.count);
	test_log(self, "  size   %" PRIu64 "\n", part->memtable_b.size);
	test_log(self, "mmap     %" PRIu64 "\n", blob_size(&part->mmap));
	test_log(self, "file     %" PRIu64 "\n", part->file.size);

	auto index = part->index;
	if (index)
	{
		test_log(self, "index\n");
		test_log(self, "  size              %" PRIu64 "\n", index->size);
		test_log(self, "  size_total        %" PRIu64 "\n", index->size_total);
		test_log(self, "  size_total_origin %" PRIu64 "\n", index->size_total_origin);
		test_log(self, "  count             %" PRIu32 "\n", index->count);
		test_log(self, "  count_total       %" PRIu64 "\n", index->count_total);
		test_log(self, "  lsn_max           %" PRIu64 "\n", index->lsn_max);
		test_log(self, "  compression       %" PRIu8  "\n", index->compression);
		for (int i = 0; i < index->count; i++)
		{
			auto region = index_get(index, i);
			auto min = index_region_min(index, i);
			auto max = index_region_max(index, i);
			test_log(self, "  region\n");
			test_log(self, "    offset          %" PRIu32 "\n", region->offset);
			test_log(self, "    size            %" PRIu32 "\n", region->size);
			test_log(self, "    count           %" PRIu32 "\n", region->count);
			test_log(self, "    min             %" PRIu64 "\n", min->time);
			test_log(self, "    max             %" PRIu64 "\n", max->time);
		}
	}

	return 0;
}

static int
test_suite_cmd_region(TestSuite* self, char* arg)
{
	char* arg_min = test_arg(&arg);
	char* arg_region = test_arg(&arg);

	if (arg_min == NULL || arg_region == NULL)
	{
		test_error(self, "line %d: region <id> <id> expected",
		           self->current_line);
		return -1;
	}
	uint64_t id = strtoull(arg_min, NULL, 10);
	uint64_t id_region = strtoull(arg_region, NULL, 10);

	auto engine = &self->env->instance.engine;
	auto part = part_find(engine, id);
	if (part == NULL)
	{
		test_log(self, "partition %" PRIu64 " not found\n", id);
		return 0;
	}

	auto index = part->index;
	if (index == NULL)
	{
		test_log(self, "partition %" PRIu64 " has no index\n", id);
		return 0;
	}

	if (id_region >= index->count)
	{
		test_log(self, "partition %" PRIu64 " region not found\n", id);
		return 0;
	}

	auto region = index_get(index, id_region);
	auto min = index_region_min(index, id_region);
	auto max = index_region_max(index, id_region);
	test_log(self, "region (index)\n");
	test_log(self, "  offset %" PRIu32 "\n", region->offset);
	test_log(self, "  size   %" PRIu32 "\n", region->size);
	test_log(self, "  count  %" PRIu32 "\n", region->count);
	test_log(self, "  min    %" PRIu64 "\n", min->time);
	test_log(self, "  max    %" PRIu64 "\n", max->time);

	Exception e;
	if (try(&e))
	{
		Buf read_buf;
		Buf read_buf_uc;

		buf_init(&read_buf);
		buf_init(&read_buf_uc);

		Reader reader;
		reader_init(&reader);
		reader.index = index;
		reader.index_region = region;
		reader.read_buf = &read_buf;
		reader.read_buf_uncompressed = &read_buf_uc;
		reader.mmap = &part->mmap;
		reader.file = &part->file;
		reader_execute(&reader);

		bool first = true;
		RegionIterator it;
		region_iterator_init(&it);
		region_iterator_open(&it, reader.region, part->comparator, NULL);
		while (region_iterator_has(&it))
		{
			auto row = region_iterator_at(&it);
			if (! first)
				test_log(self, ", ");
			test_log(self, "%" PRIu64, row->time);
			region_iterator_next(&it);
			first = false;
		}
		test_log(self, "\n");

		buf_free(&read_buf);
		buf_free(&read_buf_uc);
	}
	if (catch(&e))
	{ }

	return 0;
}

static int
test_suite_cmd_memtable(TestSuite* self, char* arg)
{
	char* arg_min = test_arg(&arg);

	if (arg_min == NULL)
	{
		test_error(self, "line %d: memtable <id> expected",
		           self->current_line);
		return -1;
	}

	uint64_t id = strtoull(arg_min, NULL, 10);

	auto engine = &self->env->instance.engine;
	auto part = part_find(engine, id);
	if (part == NULL)
	{
		test_log(self, "partition %" PRIu64 " not found\n", id);
		return 0;
	}

	test_log(self, "memtable_a (%" PRIu64 ")\n", part->memtable_a.count);

	bool first = true;
	MemtableIterator it;
	memtable_iterator_init(&it);
	memtable_iterator_open(&it, &part->memtable_a, NULL);
	while (memtable_iterator_has(&it))
	{
		auto row = memtable_iterator_at(&it);
		if (! first)
			test_log(self, ", ");
		test_log(self, "%" PRIu64, row->time);
		memtable_iterator_next(&it);
		first = false;
	}
	test_log(self, "\n");

	test_log(self, "memtable_b (%" PRIu64 ")\n", part->memtable_b.count);
	first = true;
	memtable_iterator_init(&it);
	memtable_iterator_open(&it, &part->memtable_b, NULL);
	while (memtable_iterator_has(&it))
	{
		auto row = memtable_iterator_at(&it);
		if (! first)
			test_log(self, ", ");
		test_log(self, "%" PRIu64, row->time);
		memtable_iterator_next(&it);
		first = false;
	}
	test_log(self, "\n");
	return 0;
}

static int
test_suite_cmd_memtable_rotate(TestSuite* self, char* arg)
{
	char* arg_min = test_arg(&arg);

	if (arg_min == NULL)
	{
		test_error(self, "line %d: memtable_rotate <id> expected",
		           self->current_line);
		return -1;
	}

	uint64_t id = strtoull(arg_min, NULL, 10);

	auto engine = &self->env->instance.engine;
	auto part = part_find(engine, id);
	if (part == NULL)
	{
		test_log(self, "partition %" PRIu64 " not found\n", id);
		return 0;
	}

	part_memtable_rotate(part);
	return 0;
}

static int
test_suite_cmd_compact(TestSuite* self, char* arg)
{
	char* arg_min = test_arg(&arg);

	if (arg_min == NULL)
	{
		test_error(self, "line %d: memtable_rotate <id> expected",
		           self->current_line);
		return -1;
	}

	uint64_t id = strtoull(arg_min, NULL, 10);

	auto engine = &self->env->instance.engine;
	auto part = part_find(engine, id);
	if (part == NULL)
	{
		test_log(self, "partition %" PRIu64 " not found\n", id);
		return 0;
	}

	runtime_init(&self->env->instance.global);

	Compaction compaction;
	compaction_init(&compaction);

	Exception e;
	if (try(&e))
	{
		compaction.service = engine->service;
		compaction.engine  = engine;
		compaction.global  = mn_runtime.global;

		part->service = true;

		ServiceReq req;
		service_req_init(&req);
		req.min = part->min;
		req.max = part->max;
		compaction_execute(&compaction, &req, NULL);

		compaction_free(&compaction);
	}
	if (catch(&e))
	{
		test_log_error(self);
		compaction_free(&compaction);
	}

	return 0;
}

static struct
{
	const char* name;
	int         name_size;
	int        (*function)(TestSuite*, char*);
} cmds[] =
{
	// test suite specific
	{ "trap",            4,  test_suite_cmd_trap            },
	{ "unit",            4,  test_suite_cmd_unit            },

	// api
	{ "version",         7,  test_suite_cmd_version         },
	{ "open",            4,  test_suite_cmd_open            },
	{ "close",           5,  test_suite_cmd_close           },
	{ "error",           5,  test_suite_cmd_error           },
	{ "stats",           5,  test_suite_cmd_stats           },
	{ "insert",          6,  test_suite_cmd_insert          },
	{ "delete_by",       9,  test_suite_cmd_delete_by       },
	{ "delete",          6,  test_suite_cmd_delete          },
	{ "update_by",       9,  test_suite_cmd_update_by       },
	{ "cursor_close",    12, test_suite_cmd_cursor_close    },
	{ "cursor",          6,  test_suite_cmd_cursor          },
	{ "read",            4,  test_suite_cmd_read            },
	{ "next",            4,  test_suite_cmd_next            },
	{ "select",          6,  test_suite_cmd_select          },
	{ "checkpoint",      10, test_suite_cmd_checkpoint      },
	{ "drop",            4,  test_suite_cmd_drop            },

	// internal state
	{ "files",           5,  test_suite_cmd_files           },
	{ "partitions",      10, test_suite_cmd_partitions      },
	{ "partition",       9,  test_suite_cmd_partition       },
	{ "region",          6,  test_suite_cmd_region          },
	{ "memtable_rotate", 15, test_suite_cmd_memtable_rotate },
	{ "memtable",        8,  test_suite_cmd_memtable        },
	{ "compact",         7,  test_suite_cmd_compact         },
	{  NULL,             0,  NULL                           }
};

int
test_suite_cmd(TestSuite* self, char* query)
{
	for (int i = 0; cmds[i].name; i++)
		if (! strncmp(query, cmds[i].name, cmds[i].name_size))
			return cmds[i].function(self, query + cmds[i].name_size);

	test_error(self, "line %d: unknown command: %s", query,
			   self->current_line);
	return -1;
}

#if 0
in_buf_t*
in_engine_debug_compact(in_engine_t *engine, uint64_t id, int level)
{
	in_part_t *part;
	part = in_engine_find_by_id(engine, id);
	if (in_unlikely(part == NULL))
		in_error("partition %" PRIu64 " is not found", id);

	if (level < 0 || level >= engine->level_mgr.level_count)
		in_error("%s", "level is out of bounds");

	in_merger_req_t req;
	in_merger_req_init(&req);

	in_compact(engine, engine->service.merger_mgr, &req, part, level);

	in_buf_t *buf = in_msg_begin(IN_MSG_OBJECT);

	/* array */
	in_buf_write_array(buf, 2);

	/* l */
	in_schema_t *schema = engine->schema;
	if (req.l)
	{
		/* array */
		in_buf_write_array(buf, 3);
		
		/* id */
		in_buf_write_integer(buf, req.l->id);

		if (req.l->range->count > 0)
		{
			/* min */
			in_row_t *min;
			min = in_range_min(req.l->range);
			in_buf_write(buf, in_row_of(min, schema), in_row_data_size(min));

			/* max */
			in_row_t *max;
			max = in_range_max(req.l->range);
			in_buf_write(buf, in_row_of(max, schema), in_row_data_size(max));
		} else
		{
			in_buf_write_null(buf);
			in_buf_write_null(buf);
		}

	} else
	{
		in_buf_write_null(buf);
	}

	/* r */
	if (req.r)
	{
		/* array */
		in_buf_write_array(buf, 3);

		/* id */
		in_buf_write_integer(buf, req.r->id);

		/* min */
		in_row_t *min;
		min = in_range_min(req.r->range);
		in_buf_write(buf, in_row_of(min, schema), in_row_data_size(min));

		/* max */
		in_row_t *max;
		max = in_range_max(req.r->range);
		in_buf_write(buf, in_row_of(max, schema), in_row_data_size(max));
	} else
	{
		in_buf_write_null(buf);
	}

	in_msg_end(buf);

	in_merger_req_free(&req);
	return buf;
}
#endif
