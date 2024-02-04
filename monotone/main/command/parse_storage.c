
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

static int
parse_storage_options(Lex* self, Source* config, char* command)
{
	int mask = 0;

	// (
	if (! lex_if(self, '(', NULL))
		error("%s name <(> expected", command);

	// [)]
	if (lex_if(self, ')', NULL))
		return mask;

	for (;;)
	{
		// name
		Token name;
		lex_next(self, &name);
		switch (name.id) {
		case KCLOUD:
			name.id = KNAME;
			break;
		case KNAME:
			break;
		default:
			error("%s (<name> expected", command);
			break;
		}

		// value
		if (str_compare_raw(&name.string, "path", 4))
		{
			// path <string>
			parse_string(self, &name, &config->path);
			mask |= SOURCE_PATH;
		} else
		if (str_compare_raw(&name.string, "cloud", 5))
		{
			// cloud <string>
			parse_string(self, &name, &config->cloud);
			mask |= SOURCE_CLOUD;
		} else
		if (str_compare_raw(&name.string, "refresh_wm", 10))
		{
			// refresh_wm <int>
			parse_int(self, &name, &config->refresh_wm);
			mask |= SOURCE_REFRESH_WM;
		} else
		if (str_compare_raw(&name.string, "sync", 4))
		{
			// sync <bool>
			parse_bool(self, &name, &config->sync);
			mask |= SOURCE_SYNC;
		} else
		if (str_compare_raw(&name.string, "crc", 3))
		{
			// crc <bool>
			parse_bool(self, &name, &config->crc);
			mask |= SOURCE_CRC;
		} else
		if (str_compare_raw(&name.string, "compression", 11))
		{
			// compression <int>
			parse_int(self, &name, &config->compression);
			mask |= SOURCE_COMPRESSION;
		} else
		if (str_compare_raw(&name.string, "region_size", 11))
		{
			// region_size <int>
			parse_int(self, &name, &config->region_size);
			mask |= SOURCE_REGION_SIZE;
		} else
		{
			error("%s: unknown option %.*s", command, str_size(&name.string),
			      str_of(&name.string));
		}

		// ,
		if (lex_if(self, ',', NULL))
			continue;

		// )
		if (! lex_if(self, ')', NULL))
			error("%s name (...<)> expected", command);

		break;
	}

	return mask;
}

Cmd*
parse_storage_create(Lex* self)
{
	// CREATE STORAGE [IF NOT EXISTS] name (options)
	auto cmd = cmd_storage_create_allocate();
	guard(guard, cmd_free, &cmd->cmd);

	// if not exists
	cmd->if_not_exists = parse_if_not_exists(self);

	// name
	Token name;
	if (! lex_if(self, KNAME, &name))
		error("CREATE STORAGE <name> expected");

	// create storage config
	cmd->config = source_allocate();
	source_set_name(cmd->config, &name.string);

	// (options)
	parse_storage_options(self, cmd->config, "CREATE STORAGE");

	unguard(&guard);
	return &cmd->cmd;
}

Cmd*
parse_storage_drop(Lex* self)
{
	// DROP STORAGE [IF EXISTS] name
	auto cmd = cmd_storage_drop_allocate();
	guard(guard, cmd_free, &cmd->cmd);

	// if exists
	cmd->if_exists = parse_if_exists(self);

	// name
	if (! lex_if(self, KNAME, &cmd->name))
		error("DROP STORAGE <name> expected");

	unguard(&guard);
	return &cmd->cmd;
}

static void
parse_storage_alter_rename(Lex* self, Cmd* arg)
{
	// ALTER STORAGE [IF EXISTS] name RENAME TO name
	auto cmd = cmd_storage_alter_of(arg);

	// [TO]
	lex_if(self, KTO, NULL);

	// name
	if (! lex_if(self, KNAME, &cmd->name_new))
		error("ALTER STORAGE RENAME <name> expected");
}

static void
parse_storage_alter_set(Lex* self, Cmd* arg)
{
	// ALTER STORAGE [IF EXISTS] name SET (options)
	auto cmd = cmd_storage_alter_of(arg);

	// create storage config
	cmd->config = source_allocate();
	source_set_name(cmd->config, &cmd->name.string);

	// (options)
	cmd->config_mask =
		parse_storage_options(self, cmd->config, "ALTER STORAGE");
}

Cmd*
parse_storage_alter(Lex* self)
{
	// ALTER STORAGE [IF EXISTS] name RENAME TO name
	// ALTER STORAGE [IF EXISTS] name SET (options)
	auto cmd = cmd_storage_alter_allocate();
	guard(guard, cmd_free, &cmd->cmd);

	// if exists
	cmd->if_exists = parse_if_exists(self);

	// name
	if (! lex_if(self, KNAME, &cmd->name))
		error("ALTER STORAGE <name> expected");

	// RENAME | SET
	if (lex_if(self, KRENAME, NULL))
		parse_storage_alter_rename(self, &cmd->cmd);
	else
	if (lex_if(self, KSET, NULL))
		parse_storage_alter_set(self, &cmd->cmd);
	else
		error("ALTER STORAGE name <RENAME or SET> expected");

	unguard(&guard);
	return &cmd->cmd;
}
