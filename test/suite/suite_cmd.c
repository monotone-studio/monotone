
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
		test_error(self, "line %d: cursor <name> time> [value] expected",
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

static int
test_suite_cmd_partition(TestSuite* self, char* arg)
{
	(void)self;
	(void)arg;
	return 0;
}

static int
test_suite_cmd_memtable(TestSuite* self, char* arg)
{
	(void)self;
	(void)arg;
	return 0;
}

static int
test_suite_cmd_memtable_rotate(TestSuite* self, char* arg)
{
	(void)self;
	(void)arg;
	return 0;
}

static int
test_suite_cmd_region(TestSuite* self, char* arg)
{
	(void)self;
	(void)arg;
	return 0;
}

static int
test_suite_cmd_compact(TestSuite* self, char* arg)
{
	(void)self;
	(void)arg;
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
	{ "checkpoint",      10, test_suite_cmd_checkpoint      },
	{ "drop",            4,  test_suite_cmd_drop            },

	// internal state
	{ "files",           5,  test_suite_cmd_files           },
	{ "partitions",      10, test_suite_cmd_partitions      },
	{ "partition",       9,  test_suite_cmd_partition       },
	{ "memtable_rotate", 15, test_suite_cmd_memtable_rotate },
	{ "memtable",        8,  test_suite_cmd_memtable        },
	{ "region",          6,  test_suite_cmd_region          },
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
in_engine_debug_list_files(in_engine_t *engine, int level, int level_storage)
{
	in_buf_t *buf = in_msg_begin(IN_MSG_OBJECT);

	if (level < 0 || level >= engine->level_mgr.level_count)
		in_error("%s", "level is out of bounds");

	in_level_t *ref;
	ref = in_level_of(&engine->level_mgr, level);
	if (level_storage < 0 || level_storage >= ref->storage_count)
		in_error("%s", "level storage is out of bounds");

	in_storage_t *storage;
	storage = in_level_storage_of(&engine->level_mgr, level, level_storage)->storage;

	/* <storage_path>/<storage_uuid>/<store_uuid> */
	int   storage_path_size;
	char *storage_path;
	if  (storage->path_size == 0)
	{
		in_var_t *var = in_global()->config->directory;
		storage_path_size = var->string_size;
		storage_path = var->string;
	} else
	{
		storage_path_size = storage->path_size;
		storage_path = storage->path;
	}

	char store_sz[37];
	in_uuid_to_string(engine->uuid, store_sz, sizeof(store_sz));

	char path[PATH_MAX];
	in_snprintf(path, PATH_MAX, "%.*s/%s/%s", storage_path_size, storage_path,
	            storage->uuid, store_sz);

	int array_offset = in_buf_size(buf);
	in_buf_write_array32(buf, 0);

	DIR *dir;
	dir = opendir(path);
	if (in_unlikely(dir == NULL))
		in_error("engine: directory '%s' open error", path);

	int count = 0;
	in_try
	{
		struct dirent *dir_entry;
		while ((dir_entry = readdir(dir)))
		{
			if (in_unlikely(dir_entry->d_name[0] == '.'))
				continue;
			in_buf_write_string(buf, dir_entry->d_name, strlen(dir_entry->d_name));
			count++;
		}

	} in_catch
	{
		closedir(dir);
		in_rethrow();
	}

	closedir(dir);

	uint8_t *pos = buf->start + array_offset;
	in_data_write_array32(&pos, count);

	in_msg_end(buf);
	return buf;
}

in_buf_t*
in_engine_debug_list(in_engine_t *engine)
{
	in_buf_t *buf = in_msg_begin(IN_MSG_OBJECT);

	/* array */
	in_buf_write_array(buf, engine->level_mgr.list_count);

	in_list_t *i, *n;
	in_list_foreach_safe(&engine->level_mgr.list, i, n)
	{
		in_part_t *part;
		part = in_container_of(i, in_part_t, link);
		in_buf_write_integer(buf, part->id);
	}

	in_msg_end(buf);
	return buf;
}

static inline in_part_t*
in_engine_find_by_id(in_engine_t *engine, uint64_t id)
{
	in_list_t *i;
	in_list_foreach(&engine->level_mgr.list, i)
	{
		in_part_t *part;
		part = in_container_of(i, in_part_t, link);
		if (part->id == id)
			return part;
	}
	return NULL;
}

in_buf_t*
in_engine_debug_partition(in_engine_t *engine, uint64_t id)
{
	in_part_t *part;
	part = in_engine_find_by_id(engine, id);
	if (in_unlikely(part == NULL))
		in_error("partition %" PRIu64 " is not found", id);

	in_buf_t *buf = in_msg_begin(IN_MSG_OBJECT);

	/* map */
	in_buf_write_map(buf, 9);

	/* id */
	in_buf_write_string(buf, "id", 2);
	in_buf_write_integer(buf, part->id);

	/* id_origin */
	in_buf_write_string(buf, "id_origin", 9);
	in_buf_write_integer(buf, part->id_origin);

	/* level */
	in_buf_write_string(buf, "level", 5);
	in_buf_write_integer(buf, part->level);

	/* in_memory */
	in_buf_write_string(buf, "in_memory", 9);
	in_buf_write_bool(buf, part->in_memory);

	/* memtable_a */
	in_buf_write_string(buf, "memtable_a", 10);
	in_buf_write_map(buf, 4);

	/* count */
	in_buf_write_string(buf, "count", 5);
	in_buf_write_integer(buf, part->memtable_a.count);

	/* size */
	in_buf_write_string(buf, "size", 4);
	in_buf_write_integer(buf, part->memtable_a.size);

	/* lsn_min */
	in_buf_write_string(buf, "lsn_min", 7);
	in_buf_write_integer(buf, part->memtable_a.lsn_min);

	/* lsn_max */
	in_buf_write_string(buf, "lsn_max", 8);
	in_buf_write_integer(buf, part->memtable_a.lsn_max);

	/* memtable_b */
	in_buf_write_string(buf, "memtable_b", 10);
	in_buf_write_map(buf, 4);

	/* count */
	in_buf_write_string(buf, "count", 5);
	in_buf_write_integer(buf, part->memtable_b.count);

	/* size */
	in_buf_write_string(buf, "size", 4);
	in_buf_write_integer(buf, part->memtable_b.size);

	/* lsn_min */
	in_buf_write_string(buf, "lsn_min", 7);
	in_buf_write_integer(buf, part->memtable_b.lsn_min);

	/* lsn_max */
	in_buf_write_string(buf, "lsn_max", 8);
	in_buf_write_integer(buf, part->memtable_b.lsn_max);

	/* mmap */
	in_buf_write_string(buf, "mmap", 4);
	in_buf_write_integer(buf, in_mmap_size(&part->mmap));

	/* file */
	in_buf_write_string(buf, "file", 4);
	in_buf_write_integer(buf, part->file.size);

	/* range */
	in_buf_write_string(buf, "range", 5);
	if (part->range)
	{
		/* map */
		in_buf_write_map(buf, 8);

		/* size */
		in_buf_write_string(buf, "size", 4);
		in_buf_write_integer(buf, part->range->size);

		/* size_total */
		in_buf_write_string(buf, "size_total", 10);
		in_buf_write_integer(buf, part->range->size_total);

		/* size_total_origin */
		in_buf_write_string(buf, "size_total_origin", 17);
		in_buf_write_integer(buf, part->range->size_total_origin);

		/* count */
		in_buf_write_string(buf, "count", 5);
		in_buf_write_integer(buf, part->range->count);

		/* count_total */
		in_buf_write_string(buf, "count_total", 11);
		in_buf_write_integer(buf, part->range->count_total);

		/* lsn_max */
		in_buf_write_string(buf, "lsn_max", 7);
		in_buf_write_integer(buf, part->range->lsn_max);

		/* compression */
		in_buf_write_string(buf, "compression", 11);
		in_buf_write_integer(buf, part->range->compression);

		/* regions */
		in_buf_write_string(buf, "regions", 7);
		in_buf_write_array(buf, part->range->count);

		uint32_t i = 0;
		while (i < part->range->count)
		{
			in_range_region_t *region;
			region = in_range_get(part->range, i);

			/* map */
			in_buf_write_map(buf, 5);

			/* size */
			in_buf_write_string(buf, "size", 4);
			in_buf_write_integer(buf, region->size);

			/* size_origin */
			in_buf_write_string(buf, "size_origin", 11);
			in_buf_write_integer(buf, region->size_origin);

			/* count */
			in_buf_write_string(buf, "count", 5);
			in_buf_write_integer(buf, region->count);

			/* min */
			in_buf_write_string(buf, "min", 3);
			in_row_t *min;
			min = in_range_region_min(part->range, i);
			in_buf_write(buf, in_row_of(min, part->schema), in_row_data_size(min));

			/* max */
			in_buf_write_string(buf, "max", 3);
			in_row_t *max;
			max = in_range_region_max(part->range, i);
			in_buf_write(buf, in_row_of(max, part->schema), in_row_data_size(max));

			i++;
		}

	} else
	{
		in_buf_write_null(buf);
	}

	in_msg_end(buf);
	return buf;
}

in_buf_t*
in_engine_debug_memtable(in_engine_t *engine, uint64_t id)
{
	in_part_t *part;
	part = in_engine_find_by_id(engine, id);
	if (in_unlikely(part == NULL))
		in_error("partition %" PRIu64 " is not found", id);

	in_buf_t *buf = in_msg_begin(IN_MSG_OBJECT);

	/* map */
	in_buf_write_map(buf, 2);

	/* memtable_a */
	in_buf_write_string(buf, "memtable_a", 10);
	in_buf_write_map(buf, 5);

	/* count */
	in_buf_write_string(buf, "count", 5);
	in_buf_write_integer(buf, part->memtable_a.count);

	/* size */
	in_buf_write_string(buf, "size", 4);
	in_buf_write_integer(buf, part->memtable_a.size);

	/* lsn_min */
	in_buf_write_string(buf, "lsn_min", 7);
	in_buf_write_integer(buf, part->memtable_a.lsn_min);

	/* lsn_max */
	in_buf_write_string(buf, "lsn_max", 8);
	in_buf_write_integer(buf, part->memtable_a.lsn_max);

	/* rows */
	in_buf_write_string(buf, "rows", 4);
	in_buf_write_array(buf, part->memtable_a.count);
	in_memtable_iterator_t it;
	in_memtable_iterator_init(&it);
	in_memtable_iterator_open(&it, &part->memtable_a, NULL);
	for (;;)
	{
		in_memtable_row_t *current;
		current = in_memtable_iterator_row_of(&it);
		if (current == NULL)
			break;
		in_buf_write(buf, in_row_of(current->row, part->schema),
		             in_row_data_size(current->row));
		in_memtable_iterator_next(&it);
	}

	/* memtable_b */
	in_buf_write_string(buf, "memtable_b", 10);
	in_buf_write_map(buf, 5);

	/* count */
	in_buf_write_string(buf, "count", 5);
	in_buf_write_integer(buf, part->memtable_b.count);

	/* size */
	in_buf_write_string(buf, "size", 4);
	in_buf_write_integer(buf, part->memtable_b.size);

	/* lsn_min */
	in_buf_write_string(buf, "lsn_min", 7);
	in_buf_write_integer(buf, part->memtable_b.lsn_min);

	/* lsn_max */
	in_buf_write_string(buf, "lsn_max", 8);
	in_buf_write_integer(buf, part->memtable_b.lsn_max);

	/* rows */
	in_buf_write_string(buf, "rows", 4);
	in_buf_write_array(buf, part->memtable_b.count);

	in_memtable_iterator_init(&it);
	in_memtable_iterator_open(&it, &part->memtable_b, NULL);
	for (;;)
	{
		in_memtable_row_t *current;
		current = in_memtable_iterator_row_of(&it);
		if (current == NULL)
			break;
		in_buf_write(buf, in_row_of(current->row, part->schema),
		             in_row_data_size(current->row));
		in_memtable_iterator_next(&it);
	}

	in_msg_end(buf);
	return buf;
}

in_buf_t*
in_engine_debug_memtable_rotate(in_engine_t *engine, uint64_t id)
{
	in_part_t *part;
	part = in_engine_find_by_id(engine, id);
	if (in_unlikely(part == NULL))
		in_error("partition %" PRIu64 " is not found", id);

	in_part_memtable_rotate(part);

	in_buf_t *buf = in_msg_begin(IN_MSG_OBJECT);
	in_buf_write_bool(buf, true);
	in_msg_end(buf);
	return buf;
}

in_buf_t*
in_engine_debug_region(in_engine_t *engine, uint64_t id, int order, bool in_memory)
{
	in_part_t *part;
	part = in_engine_find_by_id(engine, id);
	if (in_unlikely(part == NULL))
		in_error("partition %" PRIu64 " is not found", id);

	if (order >= (int)part->range->count)
		in_error("partition %" PRIu64 " range %d is out of bounds", id, order);

	in_range_region_t *range_region;
	range_region = in_range_get(part->range, order);

	in_reader_req_t req;
	in_reader_req_init(&req);

	in_buf_t read_buf;
	in_buf_t read_buf_uncompressed;

	in_buf_init(&read_buf);
	in_buf_init(&read_buf_uncompressed);

	req.in_memory             = in_memory;
	req.range                 = part->range;
	req.range_region          = range_region;
	req.region                = NULL;
	req.read_buf              = &read_buf;
	req.read_buf_uncompressed = &read_buf_uncompressed;
	req.mmap                  = &part->mmap;
	req.file                  = &part->file;
	in_reader_req_execute(&req);

	in_buf_t *buf = in_msg_begin(IN_MSG_OBJECT);
	in_buf_write_array(buf, req.region->count);

	in_region_iterator_t region_iterator;
	in_region_iterator_init(&region_iterator);
	in_region_iterator_open(&region_iterator, req.region, part->schema, NULL);
	for (;;)
	{
		if (! in_region_iterator_has(&region_iterator))
			break;
		in_row_t *row;
		row = in_region_iterator_read(&region_iterator);
		in_buf_write(buf, in_row_of(row, part->schema), in_row_data_size(row));
		in_region_iterator_next(&region_iterator);
	}
	in_region_iterator_free(&region_iterator);

	in_buf_unref(&read_buf);
	in_buf_unref(&read_buf_uncompressed);

	in_msg_end(buf);
	return buf;
}

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
