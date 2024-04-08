
//
// monotone.
//
// embeddable cloud-native storage for events
// and time-series data.
//
// Copyright (c) 2023-2024 Dmitry Simonenko
// Copyright (c) 2023-2024 Monotone
//
// Distributed under the terms of GNU LGPL v3 or
// Monotone Commercial License.
//

#include <monotone_runtime.h>
#include <monotone_lib.h>
#include <monotone_config.h>

typedef struct
{
	const char* name;
	VarType     type;
	int         flags;
	Var*        var;
	char*       default_string;
	uint64_t    default_int;
} ConfigDef;

static inline void
config_add(Config* self, ConfigDef* def)
{
	auto var = def->var;
	var_init(var, def->name, def->type, def->flags);
	list_append(&self->list, &var->link);
	self->count++;

	if (! (var_is(var, VAR_S) || var_is(var, VAR_H)))
		self->count_visible++;
	if (var_is(var, VAR_C))
		self->count_config++;
	if (! var_is(var, VAR_E))
	{
		list_append(&self->list_persistent, &var->link_persistent);
		self->count_persistent++;
	}

	switch (def->type) {
	case VAR_BOOL:
	case VAR_INT:
		var_int_set(var, def->default_int);
		break;
	case VAR_STRING:
		if (def->default_string)
			var_string_set_raw(var, def->default_string, strlen(def->default_string));
		break;
	case VAR_DATA:
		break;
	}
}

void
config_init(Config* self)
{
	memset(self, 0, sizeof(*self));
	list_init(&self->list);
	list_init(&self->list_persistent);
}

void
config_free(Config* self)
{
	list_foreach_safe(&self->list) {
		auto var = list_at(Var, link);
		var_free(var);
	}
}

void
config_prepare(Config* self)
{
	ConfigDef defaults[] =
	{
		// main
		{ "version",                 VAR_STRING, VAR_E,                &self->version,                 "0.0",       0                },
		{ "uuid",                    VAR_STRING, VAR_C,                &self->uuid,                    NULL,        0                },
		{ "directory",               VAR_STRING, VAR_E,                &self->directory,               NULL,        0                },
		// config
		{ "config_sync",             VAR_BOOL,   VAR_E|VAR_C|VAR_H,    &self->config_sync,             NULL,        true             },
		// log
		{ "log",                     VAR_BOOL,   VAR_C,                &self->log,                     NULL,        true             },
		{ "log_to_file",             VAR_BOOL,   VAR_C,                &self->log_to_file,             NULL,        true             },
		{ "log_to_stdout",           VAR_BOOL,   VAR_C,                &self->log_to_stdout,           NULL,        false            },
		// memory manager
		{ "mm_page_size",            VAR_INT,    VAR_C,                &self->mm_page_size,            NULL,        2097152          },
		{ "mm_limit",                VAR_BOOL,   VAR_C,                &self->mm_limit,                NULL,        false            },
		{ "mm_limit_wm",             VAR_INT,    VAR_C,                &self->mm_limit_wm,             NULL,        8589934592       },
		{ "mm_limit_behaviour",      VAR_STRING, VAR_C,                &self->mm_limit_behaviour,      "block",     0                },
		{ "mm_cache",                VAR_INT,    VAR_C,                &self->mm_cache,                NULL,        8589934592       },
		// wal
		{ "wal",                     VAR_BOOL,   VAR_C,                &self->wal,                     NULL,        true             },
		{ "wal_rotate_wm",           VAR_INT,    VAR_C|VAR_R,          &self->wal_rotate_wm,           NULL,        104857600        },
		{ "wal_sync_on_rotate",      VAR_BOOL,   VAR_C,                &self->wal_sync_on_rotate,      NULL,        true             },
		{ "wal_sync_on_write",       VAR_BOOL,   VAR_C,                &self->wal_sync_on_write,       NULL,        false            },
		{ "wal_crc",                 VAR_BOOL,   VAR_C,                &self->wal_crc,                 NULL,        false            },
		// engine
		{ "serial",                  VAR_BOOL,   VAR_C,                &self->serial,                  NULL,        true             },
		{ "interval",                VAR_INT,    VAR_C|VAR_R,          &self->interval,                NULL,        3000000          },
		{ "workers",                 VAR_INT,    VAR_C,                &self->workers,                 NULL,        3                },
		{ "workers_upload",          VAR_INT,    VAR_C,                &self->workers_upload,          NULL,        1                },
		// runtime state
		{ "ssn",                     VAR_INT,    VAR_E|VAR_R,          &self->ssn,                     NULL,        0                },
		{ "lsn",                     VAR_INT,    VAR_E,                &self->lsn,                     NULL,        0                },
		// persistent state
		{ "clouds",                  VAR_DATA,   VAR_C|VAR_H,          &self->clouds,                  NULL,        0                },
		{ "storages",                VAR_DATA,   VAR_C|VAR_H,          &self->storages,                NULL,        0                },
		{ "pipeline",                VAR_DATA,   VAR_C|VAR_H,          &self->pipeline,                NULL,        0                },
		// testing
		{ "error_refresh_1",         VAR_BOOL,   VAR_H|VAR_R|VAR_E,    &self->error_refresh_1,         NULL,        false            },
		{ "error_refresh_2",         VAR_BOOL,   VAR_H|VAR_R|VAR_E,    &self->error_refresh_2,         NULL,        false            },
		{ "error_refresh_3",         VAR_BOOL,   VAR_H|VAR_R|VAR_E,    &self->error_refresh_3,         NULL,        false            },
		{ "error_download",          VAR_BOOL,   VAR_H|VAR_R|VAR_E,    &self->error_download,          NULL,        false            },
		{ "error_upload",            VAR_BOOL,   VAR_H|VAR_R|VAR_E,    &self->error_upload,            NULL,        false            },
		{ "error_wal",               VAR_BOOL,   VAR_H|VAR_R|VAR_E,    &self->error_wal,               NULL,        false            },
		{ "test_bool",               VAR_BOOL,   VAR_H|VAR_R|VAR_E,    &self->test_bool,               NULL,        false            },
		{ "test_int",                VAR_INT,    VAR_H|VAR_R|VAR_E,    &self->test_int,                NULL,        0                },
		{ "test_string",             VAR_STRING, VAR_H|VAR_R|VAR_E,    &self->test_string,             NULL,        0                },
		{ "test_data",               VAR_DATA,   VAR_H|VAR_R|VAR_E,    &self->test_data,               NULL,        0                },
		{  NULL,                     0,             0,                 NULL,                           NULL,        0                },
	};
	for (int i = 0; defaults[i].name != NULL; i++)
		config_add(self, &defaults[i]);
}

static void
config_set_data(Config* self, uint8_t** pos)
{
	int count;
	data_read_map(pos, &count);
	for (int i = 0; i < count; i++)
	{
		// key
		Str name;
		data_read_string(pos, &name);

		// find variable and set value
		auto var = config_find(self, &name);
		if (unlikely(var == NULL))
			error("config: unknown option '%.*s'",
			      str_size(&name), str_of(&name));

		if (unlikely(! var_is(var, VAR_C)))
			error("config: option '%.*s' cannot be changed",
			      str_size(&name), str_of(&name));

		switch (var->type) {
		case VAR_BOOL:
		{
			if (unlikely(! data_is_bool(*pos)))
				error("config: bool expected for option '%.*s'",
				      str_size(&name), str_of(&name));
			bool value;
			data_read_bool(pos, &value);
			var_int_set(var, value);
			break;
		}
		case VAR_INT:
		{
			if (unlikely(! data_is_integer(*pos)))
				error("config: integer expected for option '%.*s'",
				      str_size(&name), str_of(&name));
			int64_t value;
			data_read_integer(pos, &value);
			var_int_set(var, value);
			break;
		}
		case VAR_STRING:
		{
			if (unlikely(! data_is_string(*pos)))
				error("config: string expected for option '%.*s'",
				      str_size(&name), str_of(&name));
			Str value;
			data_read_string(pos, &value);
			var_string_set(var, &value);
			break;
		}
		case VAR_DATA:
		{
			auto start = *pos;
			data_skip(pos);
			var_data_set(var, start, *pos - start);
			break;
		}
		default:
			error("config: bad option '%.*s' value",
			      str_size(&name), str_of(&name));
		}
	}
}

static void
config_set(Config* self, Str* options)
{
	Json json;
	json_init(&json);
	guard(json_free, &json);
	json_parse(&json, options);
	uint8_t* pos = json.buf.start;
	config_set_data(self, &pos);
}

static void
config_list_persistent(Config* self, Buf* buf)
{
	encode_map(buf, self->count_persistent);
	list_foreach(&self->list)
	{
		auto var = list_at(Var, link);
		if (var_is(var, VAR_E))
			continue;
		encode_string(buf, &var->name);
		var_encode(var, buf);
	}
}

static void
config_save_to(Config* self, const char* path)
{
	// get a list of variables
	Buf buf;
	buf_init(&buf);
	guard(buf_free, &buf);
	config_list_persistent(self, &buf);

	// convert to json
	Buf text;
	buf_init(&text);
	guard(buf_free, &text);
	uint8_t* pos = buf.start;
	json_export_pretty(&text, &pos);

	// create config file
	File file;
	file_init(&file);
	guard(file_close, &file);
	file_open_as(&file, path, O_CREAT|O_RDWR, 0600);
	file_write_buf(&file, &text);

	// sync
	if (var_int_of(&self->config_sync))
		file_sync(&file);
}

void
config_save(Config* self, const char* path)
{
	// remove old saved config, if exists
	Buf buf;
	buf_init(&buf);
	guard(buf_free, &buf);
	if (fs_exists("%s.old", path))
		fs_unlink("%s.old", path);

	// save existing config as a old
	fs_rename(path,  "%s.old", path);

	// create config file
	config_save_to(self, path);

	// remove old config file
	fs_unlink("%s.old", path);
}

void
config_open(Config* self, const char* path)
{
	if (fs_exists("%s", path))
	{
		Buf buf;
		buf_init(&buf);
		guard(buf_free, &buf);
		file_import(&buf, "%s", path);
		Str options;
		str_init(&options);
		buf_str(&buf, &options);
		config_set(self, &options);
		return;
	}

	// create config file
	config_save_to(self, path);
}

void
config_print(Config* self)
{
	list_foreach(&self->list)
	{
		auto var = list_at(Var, link);
		if (var_is(var, VAR_H) || var_is(var, VAR_S))
			continue;
		var_print(var);
	}
}

void
config_show(Config* self, Buf* buf)
{
	encode_map(buf, self->count_visible);
	list_foreach(&self->list)
	{
		auto var = list_at(Var, link);
		if (var_is(var, VAR_H) || var_is(var, VAR_S))
			continue;
		encode_string(buf, &var->name);
		var_encode(var, buf);
	}
}

Var*
config_find(Config* self, Str* name)
{
	list_foreach(&self->list) {
		auto var = list_at(Var, link);
		if (str_compare(&var->name, name))
			return var;
	}
	return NULL;
}
