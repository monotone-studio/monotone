
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

Cmd*
parse_show(Lex* self)
{
	// SHOW name
	int type;
	Token tk;
	lex_next(self, &tk);
	switch (tk.id) {
	case KSTORAGES:
		type = SHOW_STORAGES;
		break;
	case KPARTITIONS:
		type = SHOW_PARTITIONS;
		break;
	case KCONVEYOR:
		type = SHOW_CONVEYOR;
		break;
	case KALL:
		type = SHOW_ALL;
		break;
	case KNAME:
		type = SHOW_NAME;
		break;
	default:
		error("SHOW <STORAGES | PARTITIONS | CONVEYOR | ALL | NAME>");
	}
	return &cmd_show_allocate(type, &tk)->cmd;
}

Cmd*
parse_set(Lex* self)
{
	// SET name TO INT|STRING|BOOL

	// name
	Token name;
	if (! lex_if(self, KNAME, &name))
		error("SET <name> expected");

	// to
	if (! lex_if(self, KTO, NULL))
		error("SET name <TO> expected");

	// value
	Token value;
	lex_next(self, &value);

	return &cmd_set_allocate(&name, &value)->cmd;
}
