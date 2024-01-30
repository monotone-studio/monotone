
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
parse_partition_drop(Lex* self)
{
	// DROP PARTITION [IF EXISTS] <id>

	// if exists
	bool if_exists = parse_if_exists(self);

	// id
	Token id;
	if (! lex_if(self, KINT, &id))
		error("DROP PARTITION <id>");

	return &cmd_partition_drop_allocate(id.integer, if_exists)->cmd;
}

Cmd*
parse_partition_drop_range(Lex* self)
{
	// DROP PARTITIONS FROM <min> TO <max>

	// from
	if (! lex_if(self, KFROM, NULL))
		error("DROP PARTITIONS <FROM> min TO max");

	// min
	Token min;
	if (! lex_if(self, KINT, &min))
		error("DROP PARTITIONS FROM <min> TO max");

	// to
	if (! lex_if(self, KTO, NULL))
		error("DROP PARTITIONS FROM min <TO> max");

	Token max;
	if (! lex_if(self, KINT, &max))
		error("DROP PARTITIONS FROM min TO <max>");

	return &cmd_partition_drop_range_allocate(min.integer, max.integer)->cmd;
}

Cmd*
parse_partition_move(Lex* self)
{
	// MOVE PARTITION [IF EXISTS] <id> INTO <name>

	// if exists
	bool if_exists = parse_if_exists(self);

	// id
	Token id;
	if (! lex_if(self, KINT, &id))
		error("MOVE PARTITION <id> INTO name");

	// into
	if (! lex_if(self, KINTO, NULL))
		error("MOVE PARTITION id <INTO> name");

	// storage name
	Token name;
	if (! lex_if(self, KNAME, &name))
		error("MOVE PARTITION id INTO <name>");

	return &cmd_partition_move_allocate(id.integer, &name, if_exists)->cmd;
}

Cmd*
parse_partition_move_range(Lex* self)
{
	// MOVE PARTITIONS FROM <min> TO <max> INTO <name>

	// from
	if (! lex_if(self, KFROM, NULL))
		error("MOVE PARTITIONS <FROM> min TO max INTO name");

	// min
	Token min;
	if (! lex_if(self, KINT, &min))
		error("MOVE PARTITIONS FROM <min> TO max INTO name");

	// to
	if (! lex_if(self, KTO, NULL))
		error("MOVE PARTITIONS FROM min <TO> max INTO name");

	Token max;
	if (! lex_if(self, KINT, &max))
		error("MOVE PARTITIONS FROM min TO <max> INTO name");

	// into
	if (! lex_if(self, KINTO, NULL))
		error("MOVE PARTITIONS FROM min TO max <INTO> name");

	// storage name
	Token name;
	if (! lex_if(self, KNAME, &name))
		error("MOVE PARTITIONS FROM min TO max INTO <name>");

	return &cmd_partition_move_range_allocate(min.integer, max.integer, &name)->cmd;
}

Cmd*
parse_partition_refresh(Lex* self)
{
	// REFRESH PARTITION [IF EXISTS] <id>

	// if exists
	bool if_exists = parse_if_exists(self);

	// id
	Token id;
	if (! lex_if(self, KINT, &id))
		error("REFRESH PARTITION <id>");

	return &cmd_partition_refresh_allocate(id.integer, if_exists)->cmd;
}

Cmd*
parse_partition_refresh_range(Lex* self)
{
	// REFRESH PARTITIONS FROM <min> TO <max>

	// from
	if (! lex_if(self, KFROM, NULL))
		error("REFRESH PARTITIONS <FROM> min TO max");

	// min
	Token min;
	if (! lex_if(self, KINT, &min))
		error("REFRESH PARTITIONS FROM <min> TO max");

	// to
	if (! lex_if(self, KTO, NULL))
		error("REFRESH PARTITIONS FROM min <TO> max");

	Token max;
	if (! lex_if(self, KINT, &max))
		error("REFRESH PARTITIONS FROM min TO <max>");

	return &cmd_partition_refresh_range_allocate(min.integer, max.integer)->cmd;
}