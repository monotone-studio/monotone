
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

static struct
{
	const char* name;
	int         name_size;
	int        (*function)(TestSuite*, char*);
} cmds[] =
{
	{ "trap",         4,  test_suite_cmd_trap          },
	{ "unit",         4,  test_suite_cmd_unit          },
	{ "version",      7,  test_suite_cmd_version       },
	{ "open",         4,  test_suite_cmd_open          },
	{ "close",        5,  test_suite_cmd_close         },
	{ "error",        5,  test_suite_cmd_error         },
	{ "stats",        5,  test_suite_cmd_stats         },
	{ "insert",       6,  test_suite_cmd_insert        },
	{ "delete",       6,  test_suite_cmd_delete        },
	{ "delete_by",    9,  test_suite_cmd_delete_by     },
	{ "update_by",    9,  test_suite_cmd_update_by     },
	{ "cursor_close", 12, test_suite_cmd_cursor_close  },
	{ "cursor",       6,  test_suite_cmd_cursor        },
	{ "read",         4,  test_suite_cmd_read          },
	{ "next",         4,  test_suite_cmd_next          },
	{ "checkpoint",   10, test_suite_cmd_checkpoint    },
	{ "drop",         4,  test_suite_cmd_drop          },
	{  NULL,          0,  NULL                         }
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
