
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
#include <monotone_cloud.h>
#include <monotone_io.h>
#include <monotone_storage.h>
#include <monotone_wal.h>
#include <monotone_engine.h>
#include <monotone_command.h>
#include <monotone_main.h>

static void
execute_show_op(Executable* self, Buf* buf, CmdShowOp* op)
{
	switch (op->type) {
	case SHOW_MEMORY:
		memory_mgr_show(&self->main->memory_mgr, buf);
		break;
	case SHOW_WAL:
		wal_show(&self->main->wal, buf);
		break;
	case SHOW_CLOUDS:
	{
		Str* name = NULL;
		if (op->name.id == KNAME)
			name = &op->name.string;
		cloud_mgr_show(&self->main->cloud_mgr, name, buf);
		break;
	}
	case SHOW_STORAGES:
	{
		Str* storage = NULL;
		if (op->name.id == KNAME)
			storage = &op->name.string;
		engine_show(&self->main->engine, ENGINE_SHOW_STORAGES,
		            storage, -1, buf, op->debug);
		break;
	}
	case SHOW_PARTITION:
	{
		engine_show(&self->main->engine, ENGINE_SHOW_PARTITION,
		            NULL, op->name.integer, buf, op->debug);
		break;
	}
	case SHOW_PARTITIONS:
	{
		Str* storage = NULL;
		if (op->name.id == KNAME)
			storage = &op->name.string;
		engine_show(&self->main->engine, ENGINE_SHOW_PARTITIONS,
		            storage, -1, buf, op->debug);
		break;
	}
	case SHOW_PIPELINE:
		pipeline_show(&self->main->engine.pipeline, buf);
		break;
	case SHOW_CONFIG:
		config_show(config(), buf);
		break;
	case SHOW_NAME:
	{
		// name
		auto var = config_find(config(), &op->name.string);
		if (var && var_is(var, VAR_S))
			var = NULL;
		if (unlikely(var == NULL))
			error("SHOW name: '%.*s' not found", str_size(&op->name.string),
			      str_of(&op->name.string));
		var_encode(var, buf);
		break;
	}
	}
}

static void
execute_show(Executable* self)
{
	auto cmd = cmd_show_of(self->cmd);

	Buf buf;
	buf_init(&buf);
	guard(buf_free, &buf);

	// []
	if (cmd->list_count > 1)
		encode_array(&buf, cmd->list_count);
	list_foreach(&cmd->list)
	{
		auto op = list_at(CmdShowOp, link);
		execute_show_op(self, &buf, op);
	}

	uint8_t* pos = buf.start;
	json_export_pretty(self->output, &pos);
}

static void
execute_set(Executable* self)
{
	auto cmd  = cmd_set_of(self->cmd);
	auto name = &cmd->name.string;

	// find variable
	auto var = config_find(config(), name);
	if (var && var_is(var, VAR_S))
		var = NULL;
	if (unlikely(var == NULL))
		error("SET '%.*s': variable not found", str_size(name), str_of(name));

	// check if the variable can be updated
	if (config_online())
	{
		if (unlikely(! var_is(var, VAR_R)))
			error("SET '%.*s': variable cannot be changed online",
			      str_size(name), str_of(name));
	} else
	{
		if (unlikely(! var_is(var, VAR_C)))
			error("SET '%.*s': variable cannot be changed", str_size(name),
			      str_of(name));
	}

	// set value
	auto value = &cmd->value;
	switch (var->type) {
	case VAR_BOOL:
	{
		if (value->id != KTRUE && value->id != KFALSE)
			error("SET '%.*s': bool value expected", str_size(name),
			      str_of(name));
		bool is_true = value->id == KTRUE;
		var_int_set(var, is_true);
		break;
	}
	case VAR_INT:
	{
		if (value->id != KINT)
			error("SET '%.*s': integer value expected", str_size(name),
			      str_of(name));
		var_int_set(var, value->integer);
		break;
	}
	case VAR_STRING:
	{
		if (value->id != KSTRING)
			error("SET '%.*s': string value expected", str_size(name),
			      str_of(name));
		var_string_set(var, &value->string);
		break;
	}
	case VAR_DATA:
	{
		error("SET '%.*s': variable cannot be changed", str_size(name),
		      str_of(name));
		break;
	}
	}

	// rewrite config file
	if (config_online() && !var_is(var, VAR_E))
		control_save_config();
}

static void
execute_debug(Executable* self)
{
	auto cmd = cmd_debug_of(self->cmd);
	switch (cmd->command) {
	case DEBUG_WAL_CREATE:
		wal_rotate(&self->main->wal, 0);
		break;
	case DEBUG_WAL_GC:
		engine_gc(&self->main->engine);
		break;
	case DEBUG_WAL_READ:
	{
		WalCursor cursor;
		wal_cursor_init(&cursor);
		guard(wal_cursor_close, &cursor);
		wal_cursor_open(&cursor, &self->main->wal, 0);
		for (;;)
		{
			if (! wal_cursor_next(&cursor))
				break;
			auto write = wal_cursor_at(&cursor);
			buf_printf(self->output, "[%" PRIu64 ", %d, %d, %d]\n", write->lsn,
			           write->type,
			           write->count, write->size);
		}
		break;
	}
	case DEBUG_MEMORY_GC:
		memory_mgr_reset(&self->main->memory_mgr);
		break;
	}
}

static void
execute_checkpoint(Executable* self)
{
	engine_checkpoint(&self->main->engine);
}

static void
execute_service(Executable* self)
{
	auto engine = &self->main->engine;
	Refresh refresh;
	refresh_init(&refresh, engine);
	Exception e;
	if (enter(&e)) {
		engine_service(engine, &refresh, SERVICE_ANY, false);
	}
	refresh_free(&refresh);
	if (leave(&e))
		rethrow();
}

static void
execute_rebalance(Executable* self)
{
	auto engine = &self->main->engine;
	Refresh refresh;
	refresh_init(&refresh, engine);
	Exception e;
	if (enter(&e)) {
		engine_rebalance(engine, &refresh);
	}
	refresh_free(&refresh);
	if (leave(&e))
		rethrow();
}

static void
execute_cloud_create(Executable* self)
{
	auto cmd = cmd_cloud_create_of(self->cmd);

	// create cloud
	cloud_mgr_create(&self->main->cloud_mgr, cmd->config,
	                 cmd->if_not_exists);

	// rewrite config file
	control_save_config();
}

static void
execute_cloud_drop(Executable* self)
{
	auto cmd = cmd_cloud_drop_of(self->cmd);

	// drop storage
	cloud_mgr_drop(&self->main->cloud_mgr, &cmd->name.string,
	               cmd->if_exists);

	// rewrite config file
	control_save_config();
}

static void
execute_cloud_alter(Executable* self)
{
	auto cmd = cmd_cloud_alter_of(self->cmd);

	if (cmd->config)
	{
		// alter set
		cloud_mgr_alter(&self->main->cloud_mgr,
		                cmd->config,
		                cmd->config_mask,
		                cmd->if_exists);
	} else
	{
		// alter rename
		cloud_mgr_rename(&self->main->cloud_mgr, &cmd->name.string,
		                 &cmd->name_new.string,
		                  cmd->if_exists);
		storage_mgr_rename_cloud(&self->main->engine.storage_mgr,
		                         &cmd->name.string,
		                         &cmd->name_new.string);
	}

	// rewrite config file
	control_save_config();
}

static void
execute_storage_create(Executable* self)
{
	auto cmd = cmd_storage_create_of(self->cmd);

	// create storage
	storage_mgr_create(&self->main->engine.storage_mgr, cmd->config,
	                   cmd->if_not_exists);

	// rewrite config file
	control_save_config();
}

static void
execute_storage_drop(Executable* self)
{
	auto cmd = cmd_storage_drop_of(self->cmd);

	// drop storage
	storage_mgr_drop(&self->main->engine.storage_mgr, &cmd->name.string,
	                 cmd->if_exists);

	// rewrite config file
	control_save_config();
}

static void
execute_storage_alter(Executable* self)
{
	auto cmd = cmd_storage_alter_of(self->cmd);
	auto engine = &self->main->engine;

	if (cmd->config)
	{
		// alter set
		storage_mgr_alter(&engine->storage_mgr,
		                  cmd->config,
		                  cmd->config_mask,
		                  cmd->if_exists);
	} else
	{
		// alter rename
		storage_mgr_rename(&engine->storage_mgr, &cmd->name.string,
		                   &cmd->name_new.string,
		                    cmd->if_exists);
		pipeline_rename(&engine->pipeline, &cmd->name.string,
		                &cmd->name_new.string);
	}

	// rewrite config file
	control_save_config();
}

static void
execute_pipeline_alter(Executable* self)
{
	auto cmd = cmd_pipeline_alter_of(self->cmd);

	// alter pipeline
	pipeline_alter(&self->main->engine.pipeline, &cmd->list);

	// rewrite config file
	control_save_config();
}

static void
execute_partition_create(Executable* self)
{
	auto cmd = cmd_partition_of(self->cmd);

	// create one or more partitions
	engine_fill(&self->main->engine, cmd->min, cmd->max);
}

static void
execute_partition_drop(Executable* self)
{
	auto cmd = cmd_partition_of(self->cmd);

	// drop partition
	engine_drop(&self->main->engine, cmd->min, cmd->if_exists, cmd->mask);
}

static void
execute_partition_drop_range(Executable* self)
{
	auto cmd = cmd_partition_of(self->cmd);

	// drop partitions
	engine_drop_range(&self->main->engine, cmd->min, cmd->max, cmd->mask);
}

static void
execute_partition_move(Executable* self)
{
	auto cmd = cmd_partition_of(self->cmd);
	auto engine = &self->main->engine;

	// move partition
	Refresh refresh;
	refresh_init(&refresh, engine);
	Exception e;
	if (enter(&e)) {
		engine_refresh(engine, &refresh, cmd->min, &cmd->storage.string,
		               cmd->if_exists);
	}
	refresh_free(&refresh);
	if (leave(&e))
		rethrow();
}

static void
execute_partition_move_range(Executable* self)
{
	auto cmd = cmd_partition_of(self->cmd);
	auto engine = &self->main->engine;

	// move partitions
	Refresh refresh;
	refresh_init(&refresh, engine);
	Exception e;
	if (enter(&e)) {
		engine_refresh_range(engine, &refresh, cmd->min, cmd->max,
		                     &cmd->storage.string);
	}
	refresh_free(&refresh);
	if (leave(&e))
		rethrow();
}

static void
execute_partition_refresh(Executable* self)
{
	auto cmd = cmd_partition_of(self->cmd);
	auto engine = &self->main->engine;

	// refresh partition
	Refresh refresh;
	refresh_init(&refresh, engine);
	Exception e;
	if (enter(&e)) {
		engine_refresh(engine, &refresh, cmd->min, NULL, cmd->if_exists);
	}
	refresh_free(&refresh);
	if (leave(&e))
		rethrow();
}

static void
execute_partition_refresh_range(Executable* self)
{
	auto cmd = cmd_partition_of(self->cmd);
	auto engine = &self->main->engine;

	// refresh partitions
	Refresh refresh;
	refresh_init(&refresh, engine);
	Exception e;
	if (enter(&e)) {
		engine_refresh_range(engine, &refresh, cmd->min, cmd->max, NULL);
	}
	refresh_free(&refresh);
	if (leave(&e))
		rethrow();
}

static void
execute_partition_download(Executable* self)
{
	auto cmd = cmd_partition_of(self->cmd);

	// download partition
	engine_download(&self->main->engine, cmd->min, cmd->if_exists, cmd->if_cloud);
}

static void
execute_partition_download_range(Executable* self)
{
	auto cmd = cmd_partition_of(self->cmd);

	// drop partitions
	engine_download_range(&self->main->engine, cmd->min, cmd->max, cmd->if_cloud);
}

static void
execute_partition_upload(Executable* self)
{
	auto cmd = cmd_partition_of(self->cmd);

	// upload partition
	engine_upload(&self->main->engine, cmd->min, cmd->if_exists, cmd->if_cloud);
}

static void
execute_partition_upload_range(Executable* self)
{
	auto cmd = cmd_partition_of(self->cmd);

	// upload partitions
	engine_upload_range(&self->main->engine, cmd->min, cmd->max, cmd->if_cloud);
}

static Execute cmds[CMD_MAX] =
{
	{ NULL,                             EXECUTE_LOCK_NONE       },
	{ execute_show,                     EXECUTE_LOCK_SHARED     },
	{ execute_set,                      EXECUTE_LOCK_EXCLUSIVE  },
	{ execute_debug,                    EXECUTE_LOCK_NONE       },
	{ execute_checkpoint,               EXECUTE_LOCK_NONE       },
	{ execute_service,                  EXECUTE_LOCK_NONE       },
	{ execute_rebalance,                EXECUTE_LOCK_NONE       },
	{ execute_cloud_create,             EXECUTE_LOCK_EXCLUSIVE  },
	{ execute_cloud_drop,               EXECUTE_LOCK_EXCLUSIVE  },
	{ execute_cloud_alter,              EXECUTE_LOCK_EXCLUSIVE  },
	{ execute_storage_create,           EXECUTE_LOCK_EXCLUSIVE  },
	{ execute_storage_drop,             EXECUTE_LOCK_EXCLUSIVE  },
	{ execute_storage_alter,            EXECUTE_LOCK_EXCLUSIVE  },
	{ execute_pipeline_alter,           EXECUTE_LOCK_EXCLUSIVE  },
	{ execute_partition_create,         EXECUTE_LOCK_NONE       },
	{ execute_partition_drop,           EXECUTE_LOCK_NONE       },
	{ execute_partition_drop_range,     EXECUTE_LOCK_NONE       },
	{ execute_partition_move,           EXECUTE_LOCK_NONE       },
	{ execute_partition_move_range,     EXECUTE_LOCK_NONE       },
	{ execute_partition_refresh,        EXECUTE_LOCK_NONE       },
	{ execute_partition_refresh_range,  EXECUTE_LOCK_NONE       },
	{ execute_partition_download,       EXECUTE_LOCK_NONE       },
	{ execute_partition_download_range, EXECUTE_LOCK_NONE       },
	{ execute_partition_upload,         EXECUTE_LOCK_NONE       },
	{ execute_partition_upload_range,   EXECUTE_LOCK_NONE       }
};

void
main_execute(Main* self, const char* command, char** result)
{
	// parse
	Str text;
	str_set_cstr(&text, command);
	Lex lex;
	lex_init(&lex);
	lex_start(&lex, &text);

	auto cmd = parse(&lex);
	if (cmd == NULL)
		return;

	// prepare for execution
	auto execute = &cmds[cmd->type];
	switch (execute->lock) {
	case EXECUTE_LOCK_NONE:
		break;
	case EXECUTE_LOCK_SHARED:
		control_lock_shared();
		break;
	case EXECUTE_LOCK_EXCLUSIVE:
		control_lock_exclusive();
		break;
	}

	Buf output;
	buf_init(&output);
	guard_as(guard, buf_free, &output);

	Executable arg =
	{
		.main   = self,
		.cmd    = cmd,
		.output = &output
	};

	// execute
	Exception e;
	if (enter(&e)) {
		execute->function(&arg);
	}
	if (execute->lock != EXECUTE_LOCK_NONE)
		control_unlock();
	cmd_free(cmd);
	if (leave(&e))
		rethrow();

	if (result && buf_size(&output) > 0)
	{
		buf_write(&output, "\0", 1);
		unguard(&guard);
		*result = buf_cstr(&output);
	}
}
