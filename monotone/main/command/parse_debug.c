
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
#include <monotone_wal.h>
#include <monotone_engine.h>
#include <monotone_command.h>

Cmd*
parse_debug(Lex* self)
{
	// DEBUG command
	int id;
	Token name;
	if (! lex_if(self, KSTRING, &name))
		error("DEBUG <name> expected");

	// DEBUG WAL CREATE
	// DEBUG WAL GC
	// DEBUG WAL SHOW
	// DEBUG MEMORY GC
	if (str_compare_raw(&name.string, "wal_create", 10))
		id = DEBUG_WAL_CREATE;
	else
	if (str_compare_raw(&name.string, "wal_gc", 6))
		id = DEBUG_WAL_GC;
	else
	if (str_compare_raw(&name.string, "wal_read", 8))
		id = DEBUG_WAL_READ;
	else
	if (str_compare_raw(&name.string, "memory_gc", 9))
		id = DEBUG_MEMORY_GC;
	else
		error("DEBUG: unknown command");

	auto cmd = cmd_debug_allocate();
	cmd->command = id;
	return &cmd->cmd;
}
