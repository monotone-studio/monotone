
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

Cmd*
parse_show(Lex* self)
{
	// SHOW op [, ...]
	auto cmd = cmd_show_allocate();
	guard_as(guard, cmd_free, &cmd->cmd);

	for (;;)
	{
		// SHOW name
		int type;
		Token tk;
		lex_next(self, &tk);
		switch (tk.id) {
		case KMEMORY:
			type = SHOW_MEMORY;
			break;
		case KWAL:
			type = SHOW_WAL;
			break;
		case KCLOUD:
			// show cloud storage
			if (! lex_if(self, KNAME, &tk))
				error("SHOW CLOUD 'name' expected");
			type = SHOW_CLOUDS;
			break;
		case KCLOUDS:
			type = SHOW_CLOUDS;
			break;
		case KSTORAGE:
			// show storage storage
			if (! lex_if(self, KNAME, &tk))
				error("SHOW STORAGE 'name' expected");
			type = SHOW_STORAGES;
			break;
		case KSTORAGES:
			type = SHOW_STORAGES;
			break;
		case KPARTITION:
			// show partition id
			if (! lex_if(self, KINT, &tk))
				error("SHOW PARTITION 'id' expected");
			type = SHOW_PARTITION;
			break;
		case KPARTITIONS:
			// show partitions [storage]
			lex_if(self, KNAME, &tk);
			type = SHOW_PARTITIONS;
			break;
		case KPIPELINE:
			type = SHOW_PIPELINE;
			break;
		case KCONFIG:
		case KALL:
			// show config [name]
			if (lex_if_name(self, &tk) || lex_if(self, KSTRING, &tk)) {
				type = SHOW_NAME;
				break;
			}
			type = SHOW_CONFIG;
			break;
		case KNAME:
		case KSTRING:
			type = SHOW_NAME;
			break;
		default:
			error("SHOW 'MEMORY | WAL | CLOUDS | STORAGES | PARTITION | PARTITIONS | "
			      "PIPELINE | CONFIG | NAME'");
		}

		// [debug]
		bool debug = lex_if(self, KDEBUG, NULL);

		auto op = cmd_show_allocate_op(type, &tk, debug);
		list_append(&cmd->list, &op->link);
		cmd->list_count++;

		// ,
		if (lex_if(self, ',', NULL))
			continue;

		break;
	}

	unguard(&guard);
	return &cmd->cmd;
}

Cmd*
parse_set(Lex* self)
{
	// SET name TO INT|STRING|BOOL

	// name
	Token name;
	if (! lex_if_name(self, &name))
		error("SET 'name' expected");

	// to
	if (! lex_if(self, KTO, NULL))
		error("SET name 'TO' expected");

	// value
	Token value;
	lex_next(self, &value);

	return &cmd_set_allocate(&name, &value)->cmd;
}
