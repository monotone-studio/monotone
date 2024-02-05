
//
// monotone
//
// time-series storage
//

#include <monotone_runtime.h>
#include <monotone_lib.h>
#include <monotone_config.h>
#include <monotone_io.h>
#include <monotone_storage.h>
#include <monotone_engine.h>
#include <monotone_command.h>
#include <monotone_main.h>

static void
execute_show(Executable* self)
{
	auto cmd = cmd_show_of(self->cmd);

	switch (cmd->type) {
	case SHOW_STORAGES:
		storage_mgr_show(&self->main->engine.storage_mgr, NULL,
		                  self->output);
		break;
	case SHOW_PARTITIONS:
		storage_mgr_show_partitions(&self->main->engine.storage_mgr, NULL,
		                            self->output);
		break;
	case SHOW_CONVEYOR:
		conveyor_print(&self->main->engine.conveyor, self->output);
		break;
	case SHOW_ALL:
		config_print(config(), self->output);
		break;
	case SHOW_NAME:
	{
		// name
		auto var = config_find(config(), &cmd->name.string);
		if (var && var_is(var, VAR_S))
			var = NULL;
		if (unlikely(var == NULL))
			error("SHOW name: '%.*s' not found", str_size(&cmd->name.string),
			      str_of(&cmd->name.string));
		var_print_value(var, self->output);
		break;
	}
	}
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
execute_checkpoint(Executable* self)
{
	engine_checkpoint(&self->main->engine);
}

static void
execute_rebalance(Executable* self)
{
	auto engine = &self->main->engine;
	Refresh refresh;
	refresh_init(&refresh, engine);
	Exception e;
	if (try(&e)) {
		engine_rebalance(engine, &refresh);
	}
	refresh_free(&refresh);
	if (catch(&e))
		rethrow();
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
		conveyor_rename(&engine->conveyor, &cmd->name.string,
		                &cmd->name_new.string);
	}

	// rewrite config file
	control_save_config();
}

static void
execute_conveyor_alter(Executable* self)
{
	auto cmd = cmd_conveyor_alter_of(self->cmd);

	// alter conveyor
	conveyor_alter(&self->main->engine.conveyor, &cmd->list);

	// rewrite config file
	control_save_config();
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
	if (try(&e))
	{
		engine_sync(engine, &refresh, cmd->min, &cmd->storage.string,
		            cmd->if_exists);
	}
	refresh_free(&refresh);
	if (catch(&e))
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
	if (try(&e))
	{
		engine_sync_range(engine, &refresh, cmd->min, cmd->max,
		                  &cmd->storage.string);
	}
	refresh_free(&refresh);
	if (catch(&e))
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
	if (try(&e))
	{
		engine_refresh(engine, &refresh, cmd->min, NULL, cmd->if_exists);
	}
	refresh_free(&refresh);
	if (catch(&e))
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
	if (try(&e))
	{
		engine_refresh_range(engine, &refresh, cmd->min, cmd->max, NULL);
	}
	refresh_free(&refresh);
	if (catch(&e))
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

static void
execute_partition_sync(Executable* self)
{
	auto cmd = cmd_partition_of(self->cmd);
	auto engine = &self->main->engine;

	// sync partition
	Refresh refresh;
	refresh_init(&refresh, engine);
	Exception e;
	if (try(&e))
	{
		engine_sync(engine, &refresh, cmd->min, NULL, cmd->if_exists);
	}
	refresh_free(&refresh);
	if (catch(&e))
		rethrow();
}

static void
execute_partition_sync_range(Executable* self)
{
	auto cmd = cmd_partition_of(self->cmd);
	auto engine = &self->main->engine;

	// refresh partitions
	Refresh refresh;
	refresh_init(&refresh, engine);
	Exception e;
	if (try(&e))
	{
		engine_sync_range(engine, &refresh, cmd->min, cmd->max, NULL);
	}
	refresh_free(&refresh);
	if (catch(&e))
		rethrow();
}

static Execute cmds[CMD_MAX] =
{
	{ NULL,                             false },
	{ execute_show,                     true  },
	{ execute_set,                      true  },
	{ execute_checkpoint,               false },
	{ execute_rebalance,                false },
	{ execute_storage_create,           true  },
	{ execute_storage_drop,             true  },
	{ execute_storage_alter,            true  },
	{ execute_conveyor_alter,           true  },
	{ execute_partition_drop,           false },
	{ execute_partition_drop_range,     false },
	{ execute_partition_move,           false },
	{ execute_partition_move_range,     false },
	{ execute_partition_refresh,        false },
	{ execute_partition_refresh_range,  false },
	{ execute_partition_download,       false },
	{ execute_partition_download_range, false },
	{ execute_partition_upload,         false },
	{ execute_partition_upload_range,   false },
	{ execute_partition_sync,           false },
	{ execute_partition_sync_range,     false }
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
	if (execute->lock)
		control_lock_exclusive();

	Buf output;
	buf_init(&output);
	guard(guard, buf_free, &output);

	Executable arg =
	{
		.main   = self,
		.cmd    = cmd,
		.output = &output
	};

	// execute
	Exception e;
	if (try(&e)) {
		execute->function(&arg);
	}

	if (execute->lock)
		control_unlock();
	cmd_free(cmd);

	if (catch(&e))
		rethrow();

	if (result && buf_size(&output) > 0)
	{
		buf_write(&output, "\0", 1);
		unguard(&guard);
		*result = buf_cstr(&output);
	}
}
