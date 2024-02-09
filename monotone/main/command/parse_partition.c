
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
	// DROP PARTITION [IF EXISTS] <id> [ON STORAGE | ON CLOUD]

	// if exists
	bool if_exists = parse_if_exists(self);

	// id
	Token id;
	if (! lex_if(self, KINT, &id))
		error("DROP PARTITION <id>");

	// [on storage | on cloud]
	int mask = PART|PART_CLOUD;
	if (lex_if(self, KON, NULL))
	{
		if (lex_if(self, KSTORAGE, NULL))
			mask = PART;
		else
		if (lex_if(self, KCLOUD, NULL))
			mask = PART_CLOUD;
		else
			error("DROP PARTITION id <ON STORAGE | ON CLOUD> expected");
	}

	auto cmd = cmd_partition_allocate(CMD_PARTITION_DROP);
	cmd->min       = id.integer;
	cmd->if_exists = if_exists;
	cmd->mask      = mask;
	return &cmd->cmd;
}

Cmd*
parse_partition_drop_range(Lex* self)
{
	// DROP PARTITIONS FROM <min> TO <max> [ON STORAGE | ON CLOUD]

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

	// [on storage | on cloud]
	int mask = PART|PART_CLOUD;
	if (lex_if(self, KON, NULL))
	{
		if (lex_if(self, KSTORAGE, NULL))
			mask = PART;
		else
		if (lex_if(self, KCLOUD, NULL))
			mask = PART_CLOUD;
		else
			error("DROP PARTITION FROM min TO max <ON STORAGE | ON CLOUD> expected");
	}

	auto cmd = cmd_partition_allocate(CMD_PARTITION_DROP_RANGE);
	cmd->min  = min.integer;
	cmd->max  = max.integer;
	cmd->mask = mask;
	return &cmd->cmd;
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

	auto cmd = cmd_partition_allocate(CMD_PARTITION_MOVE);
	cmd->min       = id.integer;
	cmd->if_exists = if_exists;
	cmd->storage   = name;
	return &cmd->cmd;
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

	auto cmd = cmd_partition_allocate(CMD_PARTITION_MOVE_RANGE);
	cmd->min     = min.integer;
	cmd->max     = max.integer;
	cmd->storage = name;
	return &cmd->cmd;
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

	auto cmd = cmd_partition_allocate(CMD_PARTITION_REFRESH);
	cmd->min       = id.integer;
	cmd->if_exists = if_exists;
	return &cmd->cmd;
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

	auto cmd = cmd_partition_allocate(CMD_PARTITION_REFRESH_RANGE);
	cmd->min = min.integer;
	cmd->max = max.integer;
	return &cmd->cmd;
}

Cmd*
parse_partition_download(Lex* self)
{
	// DOWNLOAD PARTITION [IF EXISTS] <id> [IF CLOUD]

	// if exists
	bool if_exists = parse_if_exists(self);

	// id
	Token id;
	if (! lex_if(self, KINT, &id))
		error("DOWNLOAD PARTITION <id>");

	// [if cloud]
	bool if_cloud = parse_if_cloud(self);

	auto cmd = cmd_partition_allocate(CMD_PARTITION_DOWNLOAD);
	cmd->min       = id.integer;
	cmd->if_exists = if_exists;
	cmd->if_cloud  = if_cloud;
	return &cmd->cmd;
}

Cmd*
parse_partition_download_range(Lex* self)
{
	// DOWNLOAD PARTITIONS FROM <min> TO <max> [IF CLOUD]

	// from
	if (! lex_if(self, KFROM, NULL))
		error("DOWNLOAD PARTITIONS <FROM> min TO max");

	// min
	Token min;
	if (! lex_if(self, KINT, &min))
		error("DOWNLOAD PARTITIONS FROM <min> TO max");

	// to
	if (! lex_if(self, KTO, NULL))
		error("DOWNLOAD PARTITIONS FROM min <TO> max");

	Token max;
	if (! lex_if(self, KINT, &max))
		error("DOWNLOAD PARTITIONS FROM min TO <max>");

	// [if cloud]
	bool if_cloud = parse_if_cloud(self);

	auto cmd = cmd_partition_allocate(CMD_PARTITION_DOWNLOAD_RANGE);
	cmd->min      = min.integer;
	cmd->max      = max.integer;
	cmd->if_cloud = if_cloud;
	return &cmd->cmd;
}

Cmd*
parse_partition_upload(Lex* self)
{
	// UPLOAD PARTITION [IF EXISTS] <id> [IF CLOUD]

	// if exists
	bool if_exists = parse_if_exists(self);

	// id
	Token id;
	if (! lex_if(self, KINT, &id))
		error("UPLOAD PARTITION <id>");

	// [if cloud]
	bool if_cloud = parse_if_cloud(self);

	auto cmd = cmd_partition_allocate(CMD_PARTITION_UPLOAD);
	cmd->min       = id.integer;
	cmd->if_exists = if_exists;
	cmd->if_cloud  = if_cloud;
	return &cmd->cmd;
}

Cmd*
parse_partition_upload_range(Lex* self)
{
	// UPLOAD PARTITIONS FROM <min> TO <max> [IF CLOUD]

	// from
	if (! lex_if(self, KFROM, NULL))
		error("UPLOAD PARTITIONS <FROM> min TO max");

	// min
	Token min;
	if (! lex_if(self, KINT, &min))
		error("UPLOAD PARTITIONS FROM <min> TO max");

	// to
	if (! lex_if(self, KTO, NULL))
		error("UPLOAD PARTITIONS FROM min <TO> max");

	Token max;
	if (! lex_if(self, KINT, &max))
		error("UPLOAD PARTITIONS FROM min TO <max>");

	// [if cloud]
	bool if_cloud = parse_if_cloud(self);

	auto cmd = cmd_partition_allocate(CMD_PARTITION_UPLOAD_RANGE);
	cmd->min      = min.integer;
	cmd->max      = max.integer;
	cmd->if_cloud = if_cloud;
	return &cmd->cmd;
}
