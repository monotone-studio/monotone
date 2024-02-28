
//
// monotone
//
// time-series storage
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

static int
parse_cloud_options(Lex* self, CloudConfig* config, char* command)
{
	// [(]
	int mask = 0;
	if (! lex_if(self, '(', NULL))
		return mask;

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
		case KDEBUG:
			name.id = KNAME;
			break;
		case KNAME:
			break;
		default:
			error("%s (<name> expected", command);
			break;
		}

		// value
		if (str_compare_raw(&name.string, "type", 4))
		{
			// type <string>
			parse_string(self, &name, &config->type);
			mask |= CLOUD_TYPE;
		} else
		if (str_compare_raw(&name.string, "url", 3))
		{
			// url <string>
			parse_string(self, &name, &config->url);
			mask |= CLOUD_URL;
		} else
		if (str_compare_raw(&name.string, "login", 5))
		{
			// login <string>
			parse_string(self, &name, &config->login);
			mask |= CLOUD_LOGIN;
		} else
		if (str_compare_raw(&name.string, "password", 8))
		{
			// password <string>
			parse_string(self, &name, &config->password);
			mask |= CLOUD_PASSWORD;
		} else
		if (str_compare_raw(&name.string, "access_key", 10))
		{
			// access_key <string>
			parse_string(self, &name, &config->login);
			mask |= CLOUD_LOGIN;
		} else
		if (str_compare_raw(&name.string, "secret_key", 10))
		{
			// password <string>
			parse_string(self, &name, &config->password);
			mask |= CLOUD_PASSWORD;
		} else
		if (str_compare_raw(&name.string, "debug", 5))
		{
			// debug <bool>
			parse_bool(self, &name, &config->debug);
			mask |= CLOUD_DEBUG;
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
parse_cloud_create(Lex* self)
{
	// CREATE CLOUD [IF NOT EXISTS] name (options)
	auto cmd = cmd_cloud_create_allocate();
	guard(guard, cmd_free, &cmd->cmd);

	// if not exists
	cmd->if_not_exists = parse_if_not_exists(self);

	// name
	Token name;
	if (! lex_if(self, KNAME, &name))
		error("CREATE CLOUD <name> expected");

	// create cloud config
	cmd->config = cloud_config_allocate();
	cloud_config_set_name(cmd->config, &name.string);

	// [(options)]
	parse_cloud_options(self, cmd->config, "CREATE CLOUD");

	unguard(&guard);
	return &cmd->cmd;
}

Cmd*
parse_cloud_drop(Lex* self)
{
	// DROP CLOUD [IF EXISTS] name
	auto cmd = cmd_cloud_drop_allocate();
	guard(guard, cmd_free, &cmd->cmd);

	// if exists
	cmd->if_exists = parse_if_exists(self);

	// name
	if (! lex_if(self, KNAME, &cmd->name))
		error("DROP CLOUD <name> expected");

	unguard(&guard);
	return &cmd->cmd;
}

static void
parse_cloud_alter_rename(Lex* self, Cmd* arg)
{
	// ALTER CLOUD [IF EXISTS] name RENAME TO name
	auto cmd = cmd_cloud_alter_of(arg);

	// [TO]
	lex_if(self, KTO, NULL);

	// name
	if (! lex_if(self, KNAME, &cmd->name_new))
		error("ALTER CLOUD RENAME <name> expected");
}

static void
parse_cloud_alter_set(Lex* self, Cmd* arg)
{
	// ALTER CLOUD [IF EXISTS] name SET (options)
	auto cmd = cmd_cloud_alter_of(arg);

	// create cloud config
	cmd->config = cloud_config_allocate();
	cloud_config_set_name(cmd->config, &cmd->name.string);

	// (options)
	cmd->config_mask =
		parse_cloud_options(self, cmd->config, "ALTER CLOUD");

	// do not allow to change cloud type
	if (cmd->config_mask & CLOUD_TYPE)
		error("cloud type cannot be changed");
}

Cmd*
parse_cloud_alter(Lex* self)
{
	// ALTER CLOUD [IF EXISTS] name RENAME TO name
	// ALTER CLOUD [IF EXISTS] name SET (options)
	auto cmd = cmd_cloud_alter_allocate();
	guard(guard, cmd_free, &cmd->cmd);

	// if exists
	cmd->if_exists = parse_if_exists(self);

	// name
	if (! lex_if(self, KNAME, &cmd->name))
		error("ALTER CLOUD <name> expected");

	// RENAME | SET
	if (lex_if(self, KRENAME, NULL))
		parse_cloud_alter_rename(self, &cmd->cmd);
	else
	if (lex_if(self, KSET, NULL))
		parse_cloud_alter_set(self, &cmd->cmd);
	else
		error("ALTER CLOUD name <RENAME or SET> expected");

	unguard(&guard);
	return &cmd->cmd;
}
