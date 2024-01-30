
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
	Token name;
	if (! lex_if(self, KNAME, &name))
		error("SHOW <name> expected");
	return &cmd_show_allocate(&name)->cmd;
}

Cmd*
parse_set(Lex* self)
{
	// SET name TO INT|STRING

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
	if (value.id != KINT && value.id != KSTRING)
		error("SET name TO <value> expected string or int");

	return &cmd_set_allocate(&name, &value)->cmd;
}
