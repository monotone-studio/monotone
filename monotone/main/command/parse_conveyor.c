
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

static void
parse_conveyor_set(Lex* self, Cmd* arg)
{
	auto cmd = cmd_conveyor_alter_of(arg);

	// storage [(options]), ...]
	for (;;)
	{
		// storage name
		Token name;
		if (! lex_if(self, KNAME, &name))
			error("ALTER CONVEYOR SET <storage name> expected");

		// create tier config
		auto config = tier_config_allocate();
		guard(guard, tier_config_free, config);
		tier_config_set_name(config, &name.string);

		// ,
		if (lex_if(self, ',', NULL))
		{
			unguard(&guard);
			cmd_conveyor_alter_add(cmd, config);
			continue;
		}

		// eof
		if (lex_if(self, KEOF, NULL))
		{
			unguard(&guard);
			cmd_conveyor_alter_add(cmd, config);
			break;
		}

		// (
		if (! lex_if(self, '(', NULL))
			error("ALTER CONVEYOR SET name <(> expected");

		// [)]
		if (lex_if(self, ')', NULL))
		{
			unguard(&guard);
			cmd_conveyor_alter_add(cmd, config);
			continue;
		}

		// (tier options)
		for (;;)
		{
			// name
			if (! lex_if(self, KNAME, &name))
				error("ALTER CONVEYOR SET name (<name> value) expected");

			// value
			if (str_compare_raw(&name.string, "capacity", 8))
			{
				// capacity <int>
				parse_int(self, &name, &config->capacity);
			} else
			{
				error("ALTER CONVEYOR: unknown option %.*s", str_size(&name.string),
				      str_of(&name.string));
			}

			// ,
			if (lex_if(self, ',', NULL))
				continue;

			// )
			if (! lex_if(self, ')', NULL))
				error("ALTER CONVEYOR SET name (...<)> expected");

			break;
		}
		unguard(&guard);
		cmd_conveyor_alter_add(cmd, config);

		// ,
		if (lex_if(self, ',', NULL))
			continue;

		// eof
		if (lex_if(self, KEOF, NULL))
			break;

		error("ALTER CONVEYOR SET name() <,> expected");
	}
}

Cmd*
parse_conveyor_alter(Lex* self)
{
	// ALTER CONVEYOR RESET
	// ALTER CONVEYOR SET storage_name (tier_options) [, ...]
	auto cmd = cmd_conveyor_alter_allocate();
	guard(guard, cmd_free, &cmd->cmd);

	// RESET | SET
	if (lex_if(self, KRESET, NULL)) {
		// empty list
	} else
	{
		if (! lex_if(self, KSET, NULL))
			error("ALTER CONVEYOR <SET | RESET> expected");

		// SET storage (options), ...
		parse_conveyor_set(self, &cmd->cmd);
	}

	unguard(&guard);
	return &cmd->cmd;
}