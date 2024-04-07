
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

static void
parse_pipeline_set(Lex* self, Cmd* arg)
{
	auto cmd = cmd_pipeline_alter_of(arg);

	// storage [(options]), ...]
	for (;;)
	{
		// storage name
		Token name;
		if (! lex_if(self, KNAME, &name))
			error("ALTER PIPELINE SET 'storage name' expected");

		// create tier config
		auto config = tier_config_allocate();
		guard_as(guard, tier_config_free, config);
		tier_config_set_name(config, &name.string);

		// ,
		if (lex_if(self, ',', NULL))
		{
			unguard(&guard);
			cmd_pipeline_alter_add(cmd, config);
			continue;
		}

		// eof
		if (lex_if(self, KEOF, NULL))
		{
			unguard(&guard);
			cmd_pipeline_alter_add(cmd, config);
			break;
		}

		// (
		if (! lex_if(self, '(', NULL))
			error("ALTER PIPELINE SET name '(' expected");

		// [)]
		if (lex_if(self, ')', NULL))
		{
			unguard(&guard);
			cmd_pipeline_alter_add(cmd, config);
			continue;
		}

		// (tier options)
		for (;;)
		{
			// name
			lex_next(self, &name);
			switch (name.id) {
			case KPARTITIONS:
				name.id = KNAME;
				break;
			case KNAME:
				break;
			default:
				error("ALTER PIPELINE SET name ('name' value) expected");
				break;
			}

			// value
			if (str_compare_raw(&name.string, "partitions", 10))
			{
				// partitions <int>
				parse_int(self, &name, &config->partitions);
			} else
			if (str_compare_raw(&name.string, "size", 4))
			{
				// size <int>
				parse_int(self, &name, &config->size);
			} else
			if (str_compare_raw(&name.string, "events", 6))
			{
				// events <int>
				parse_int(self, &name, &config->events);
			} else
			if (str_compare_raw(&name.string, "duration", 8))
			{
				// duration <int>
				parse_int(self, &name, &config->duration);
			} else
			{
				error("ALTER PIPELINE: unknown option '%.*s'", str_size(&name.string),
				      str_of(&name.string));
			}

			// ,
			if (lex_if(self, ',', NULL))
				continue;

			// )
			if (! lex_if(self, ')', NULL))
				error("ALTER PIPELINE SET name (...')' expected");

			break;
		}
		unguard(&guard);
		cmd_pipeline_alter_add(cmd, config);

		// ,
		if (lex_if(self, ',', NULL))
			continue;

		// eof
		if (lex_if(self, KEOF, NULL))
			break;

		error("ALTER PIPELINE SET name() ',' expected");
	}
}

Cmd*
parse_pipeline_alter(Lex* self)
{
	// ALTER PIPELINE RESET
	// ALTER PIPELINE SET storage_name (tier_options) [, ...]
	auto cmd = cmd_pipeline_alter_allocate();
	guard_as(guard, cmd_free, &cmd->cmd);

	// RESET | SET
	if (lex_if(self, KRESET, NULL)) {
		// empty list
	} else
	{
		if (! lex_if(self, KSET, NULL))
			error("ALTER PIPELINE 'SET | RESET' expected");

		// SET storage (options), ...
		parse_pipeline_set(self, &cmd->cmd);
	}

	unguard(&guard);
	return &cmd->cmd;
}
