
//
// monotone
//
// time-series storage
//

#include <monotone_runtime.h>
#include <monotone_lib.h>
#include <monotone_io.h>
#include <monotone_storage.h>
#include <monotone_engine.h>
#include <monotone_main.h>

static void
execute_set(Lex* lex)
{
	// SET name TO INT|STRING
	Token name;
	if (! lex_if(lex, KNAME, &name))
		error("SET <name> expected");
	// TO
	if (! lex_if(lex, KTO, NULL))
		error("SET name <TO> expected");

	// value
	Token value;
	lex_next(lex, &value);

	// find variable
	auto var = config_find(config(), &name.string);
	if (var && var_is(var, VAR_S))
		var = NULL;
	if (unlikely(var == NULL))
		error("SET '%.*s': variable not found", str_size(&name.string),
		      str_of(&name.string));

	// check if the variable can be updated
	if (config_online())
	{
		if (unlikely(! var_is(var, VAR_R)))
			error("SET '%.*s': variable cannot be changed online",
			      str_size(&name.string), str_of(&name.string));
	} else
	{
		if (unlikely(! var_is(var, VAR_C)))
			error("SET '%.*s': variable cannot be changed", str_size(&name.string),
			      str_of(&name.string));
	}

	// set value
	switch (var->type) {
	case VAR_BOOL:
	{
		if (value.id != KTRUE && value.id != KFALSE)
			error("SET '%.*s': bool value expected", str_size(&name.string),
			      str_of(&name.string));
		bool is_true = value.id == KTRUE;
		var_int_set(var, is_true);
		break;
	}
	case VAR_INT:
	{
		if (value.id != KINT)
			error("SET '%.*s': integer value expected", str_size(&name.string),
			      str_of(&name.string));
		var_int_set(var, value.integer);
		break;
	}
	case VAR_STRING:
	{
		if (value.id != KSTRING)
			error("SET '%.*s': string value expected", str_size(&name.string),
			      str_of(&name.string));
		var_string_set(var, &value.string);
		break;
	}
	case VAR_DATA:
	{
		error("SET '%.*s': variable cannot be changed", str_size(&name.string),
		      str_of(&name.string));
		break;
	}
	}

	// rewrite config file
	if (config_online() && !var_is(var, VAR_E))
		config_update();
}

static void
execute_show(Main* self, Lex* lex, Buf* output)
{
	// SHOW [STORAGES | CONVEYOR | ALL | name]

	// storages
	if (lex_if(lex, KSTORAGES, NULL))
	{
		engine_storage_show_all(&self->engine, output);
		return;
	}

	// storage <name>
	if (lex_if(lex, KSTORAGE, NULL))
	{
		Token name;
		if (! lex_if(lex, KNAME, &name))
			error("SHOW STORAGE <name> expected");

		engine_storage_show(&self->engine, &name.string, output);
		return;
	}

	// conveyor
	if (lex_if(lex, KCONVEYOR, NULL))
	{
		engine_conveyor_show(&self->engine, output);
		return;
	}

	// all
	if (lex_if(lex, KALL, NULL))
	{
		config_print(config(), output);
		return;
	}

	// SHOW name
	Token name;
	if (! lex_if(lex, KNAME, &name))
		error("SHOW <name> expected");

	auto var = config_find(config(), &name.string);
	if (var && var_is(var, VAR_S))
		var = NULL;
	if (unlikely(var == NULL))
		error("SHOW name: '%.*s' not found", str_size(&name.string),
		      str_of(&name.string));
	var_print_value(var, output);
}

static void
execute_checkpoint(Main* self, Lex* lex)
{
	// CHECKPOINT
	unused(lex);
	engine_checkpoint(&self->engine);
}

static inline bool
parse_if_not_exists(Lex* self)
{
	if (! lex_if(self, KIF, NULL))
		return false;
	if (! lex_if(self, KNOT, NULL))
		error("IF <NOT> EXISTS expected");
	if (! lex_if(self, KEXISTS, NULL))
		error("IF NOT <EXISTS> expected");
	return true;
}

static inline bool
parse_if_exists(Lex* self)
{
	if (! lex_if(self, KIF, NULL))
		return false;
	if (! lex_if(self, KEXISTS, NULL))
		error("IF <EXISTS> expected");
	return true;
}

static inline void
parse_int(Lex* lex, Token* name, int64_t* value)
{
	// int
	Token tk;
	if (! lex_if(lex, KINT, &tk))
		error("%.*s <integer> expected", str_size(&name->string),
		      str_of(&name->string));

	*value = tk.integer;
}

static inline void
parse_bool(Lex* lex, Token* name, bool* value)
{
	// [true/false]
	Token tk;
	if (! lex_if(lex, KNAME, &tk))
	{
		*value = true;
		return;
	}
	if (str_compare_raw(&tk.string, "true", 4))
		*value = true;
	else
	if (str_compare_raw(&tk.string, "false", 5))
		*value = false;
	else
		error("%.*s <true/false> expected", str_size(&name->string),
		      str_of(&name->string));
}

static inline void
parse_string(Lex* lex, Token* name, Str* value)
{
	// string
	Token tk;
	if (! lex_if(lex, KSTRING, &tk))
		error("%.*s <string> expected", str_size(&name->string),
		      str_of(&name->string));

	str_free(value);
	str_copy(value, &tk.string);
}

static void
execute_storage_create(Main* self, Lex* lex)
{
	// CREATE STORAGE [IF NOT EXISTS] name (options)
	bool if_not_exists = parse_if_not_exists(lex);

	// name
	Token name;
	if (! lex_if(lex, KNAME, &name))
		error("CREATE STORAGE <name> expected");

	// create storage target
	auto target = target_allocate();
	guard(guard, target_free, target);
	target_set_name(target, &name.string);

	if (lex_if(lex, KEOF, NULL))
		goto create;

	// (
	if (! lex_if(lex, '(', NULL))
		error("CREATE STORAGE name <(> expected");

	// [)]
	if (lex_if(lex, ')', NULL))
		goto create;

	for (;;)
	{
		// key
		if (! lex_if(lex, KNAME, &name))
			error("CREATE STORAGE (<name> expected");

		// value
		if (str_compare_raw(&name.string, "path", 4))
		{
			// path <string>
			parse_string(lex, &name, &target->path);
		} else
		if (str_compare_raw(&name.string, "compaction_wm", 13))
		{
			// compaction_wm <int>
			parse_int(lex, &name, &target->compaction_wm);
		} else
		if (str_compare_raw(&name.string, "sync", 4))
		{
			// sync <bool>
			parse_bool(lex, &name, &target->sync);
		} else
		if (str_compare_raw(&name.string, "crc", 3))
		{
			// crc <bool>
			parse_bool(lex, &name, &target->crc);
		} else
		if (str_compare_raw(&name.string, "compression", 11))
		{
			// compression <int>
			parse_int(lex, &name, &target->compression);
		} else
		if (str_compare_raw(&name.string, "region_size", 11))
		{
			// region_size <int>
			parse_int(lex, &name, &target->region_size);
		} else
		{
			error("CREATE STORAGE: unknown option %.*s", str_size(&name.string),
			      str_of(&name.string));
		}

		// ,
		if (lex_if(lex, ',', NULL))
			continue;

		// )
		if (! lex_if(lex, ')', NULL))
			error("CREATE STORAGE name (...<)> expected");

		break;
	}

create:
	// set default storage path, if not set
	if (str_empty(&target->path))
	{
		char path[PATH_MAX];
		snprintf(path, sizeof(path), "%s/%s", config_directory(),
		         str_of(&target->name));
		str_strdup(&target->path, path);
	}

	// create storage
	engine_storage_create(&self->engine, target, if_not_exists);
}

static void
execute_storage_drop(Main* self, Lex* lex)
{
	// DROP STORAGE [IF EXISTS] name
	bool if_exists = parse_if_exists(lex);

	// name
	Token name;
	if (! lex_if(lex, KNAME, &name))
		error("DROP STORAGE <name> expected");

	// drop storage
	engine_storage_drop(&self->engine, &name.string, if_exists);
}

static void
free_configs(List* list)
{
	list_foreach_safe(list)
	{
		auto config = list_at(TierConfig, link);
		tier_config_free(config);
	}
}

static void
execute_conveyor_alter(Main* self, Lex* lex)
{
	// ALTER CONVEYOR storage_name (tier_options) [, ...]
	List configs;
	list_init(&configs);
	guard(configs_guard, free_configs, &configs);

	// reset conveyor (no storage list)
	if (lex_if(lex, KEOF, NULL))
	{
		engine_conveyor_alter(&self->engine, &configs);
		return;
	}

	for (;;)
	{
		// storage name
		Token name;
		if (! lex_if(lex, KNAME, &name))
			error("ALTER CONVEYOR <storage name> expected");

		// create tier config
		auto config = tier_config_allocate();
		guard(config_guard, tier_config_free, config);
		tier_config_set_name(config, &name.string);

		// ,
		if (lex_if(lex, ',', NULL))
		{
			unguard(&config_guard);
			list_append(&configs, &config->link);
			continue;
		}

		// eof
		if (lex_if(lex, KEOF, NULL))
		{
			unguard(&config_guard);
			list_append(&configs, &config->link);
			break;
		}

		// (
		if (! lex_if(lex, '(', NULL))
			error("ALTER CONVEYOR storage_name <(> expected");

		// [)]
		if (lex_if(lex, ')', NULL))
		{
			unguard(&config_guard);
			list_append(&configs, &config->link);
			continue;
		}

		// (tier options)
		for (;;)
		{
			// name
			if (! lex_if(lex, KNAME, &name))
				error("ALTER CONVEYOR namme (<name> value) expected");

			// value
			if (str_compare_raw(&name.string, "capacity", 8))
			{
				// capacity <int>
				parse_int(lex, &name, &config->capacity);
			} else
			{
				error("ALTER CONVEYOR: unknown option %.*s", str_size(&name.string),
				      str_of(&name.string));
			}

			// ,
			if (lex_if(lex, ',', NULL))
				continue;

			// )
			if (! lex_if(lex, ')', NULL))
				error("ALTER CONVEYOR name (...<)> expected");

			break;
		}
	}

	// alter conveyor
	engine_conveyor_alter(&self->engine, &configs);
}

static void
execute_partition_move(Main* self, Lex* lex)
{
	// MOVE PARTITION <id> INTO <name>

	// id
	Token id;
	if (! lex_if(lex, KINT, &id))
		error("MOVE PARTITION <id> INTO name");

	// into
	if (! lex_if(lex, KINTO, NULL))
		error("MOVE PARTITION id <INTO> name");

	// storage name
	Token name;
	if (! lex_if(lex, KNAME, &name))
		error("MOVE PARTITION id INTO <name>");

	// move partition
	engine_partition_move(&self->engine, id.integer, &name.string);
}

static void
execute_partition_drop(Main* self, Lex* lex)
{
	// DROP PARTITION <id>

	// id
	Token id;
	if (! lex_if(lex, KINT, &id))
		error("MOVE PARTITION <id> INTO name");

	// drop partition
	engine_partition_drop(&self->engine, id.integer);
}


static void
execute_partitions_move(Main* self, Lex* lex)
{
	// MOVE PARTITIONS FROM <min> TO <max> INTO <name>

	// from
	if (! lex_if(lex, KFROM, NULL))
		error("MOVE PARTITIONS <FROM> min TO max INTO name");

	// min
	Token min;
	if (! lex_if(lex, KINT, &min))
		error("MOVE PARTITIONS FROM <min> TO max INTO name");

	// to
	if (! lex_if(lex, KTO, NULL))
		error("MOVE PARTITIONS FROM min <TO> max INTO name");

	Token max;
	if (! lex_if(lex, KINT, &max))
		error("MOVE PARTITIONS FROM min TO <max> INTO name");

	// into
	if (! lex_if(lex, KINTO, NULL))
		error("MOVE PARTITIONS FROM min TO max <INTO> name");

	// storage name
	Token name;
	if (! lex_if(lex, KNAME, &name))
		error("MOVE PARTITIONS FROM min TO max INTO <name>");

	// move partitions
	engine_partitions_move(&self->engine, min.integer, max.integer,
	                       &name.string);
}

static void
execute_partitions_drop(Main* self, Lex* lex)
{
	// DROP PARTITIONS FROM <min> TO <max>

	// from
	if (! lex_if(lex, KFROM, NULL))
		error("MOVE PARTITIONS <FROM> min TO max");

	// min
	Token min;
	if (! lex_if(lex, KINT, &min))
		error("MOVE PARTITIONS FROM <min> TO max");

	// to
	if (! lex_if(lex, KTO, NULL))
		error("MOVE PARTITIONS FROM min <TO> max");

	Token max;
	if (! lex_if(lex, KINT, &max))
		error("MOVE PARTITIONS FROM min TO <max>");

	// drop partitions
	engine_partitions_drop(&self->engine, min.integer, max.integer);
}

void
main_execute(Main* self, const char* command, char** result)
{
	Buf output;
	buf_init(&output);
	guard(guard, buf_free, &output);

	Str text;
	str_set_cstr(&text, command);

	Lex lex;
	lex_init(&lex);
	lex_start(&lex, &text);

	if (result)
		*result = NULL;

	Token tk;
	lex_next(&lex, &tk);
	switch (tk.id) {
	case KSET:
		execute_set(&lex);
		break;
	case KSHOW:
		execute_show(self, &lex, &output);
		break;
	case KCHECKPOINT:
		execute_checkpoint(self, &lex);
		break;
	case KCREATE:
	{
		if (! config_online())
			error("storage is not online");

		// CREATE STORAGE
		if (lex_if(&lex, KSTORAGE, NULL))
			execute_storage_create(self, &lex);
		else
			error("CREATE <STORAGE> expected");
		break;
	}
	case KDROP:
	{
		if (! config_online())
			error("storage is not online");

		// DROP STORAGE | PARTITION | PARTITIONS
		if (lex_if(&lex, KSTORAGE, NULL))
			execute_storage_drop(self, &lex);
		else
		if (lex_if(&lex, KPARTITION, NULL))
			execute_partition_drop(self, &lex);
		else
		if (lex_if(&lex, KPARTITIONS, NULL))
			execute_partitions_drop(self, &lex);
		else
			error("DROP <STORAGE|PARTITION|PARTITIONS> expected");
		break;
	}
	case KALTER:
	{
		if (! config_online())
			error("storage is not online");

		// ALTER STORAGE | CONVEYOR
		if (lex_if(&lex, KSTORAGE, NULL))
			; // todo
		else
		if (lex_if(&lex, KCONVEYOR, NULL))
			execute_conveyor_alter(self, &lex);
		else
			error("ALTER <STORAGE|CONVEYOR> expected");
		break;
	}
	case KMOVE:
	{
		if (! config_online())
			error("storage is not online");

		// MOVE PARTITION | PARTITIONS
		if (lex_if(&lex, KPARTITION, NULL))
			execute_partition_move(self, &lex);
		else
		if (lex_if(&lex, KPARTITIONS, NULL))
			execute_partitions_move(self, &lex);
		else
			error("MOVE <PARTITION|PARTITIONS> expected");
		break;
	}
	case KEOF:
		break;
	default:
		error("unknown command");
		break;
	}

	if (result && buf_size(&output) > 0)
	{
		buf_write(&output, "\0", 1);
		unguard(&guard);
		*result = buf_cstr(&output);
	}
}
