
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
parse(Lex* self)
{
	Cmd* cmd = NULL;

	Token tk;
	lex_next(self, &tk);
	switch (tk.id) {
	case KSET:
	{
		// SET name TO value
		cmd = parse_set(self);
		break;
	}
	case KSHOW:
	{
		// SHOW name
		cmd = parse_show(self);
		break;
	}
	case KDEBUG:
	{
		// DEBUG command
		cmd = parse_debug(self);
		break;
	}
	case KCHECKPOINT:
	{
		// CHECKPOINT
		cmd = parse_checkpoint(self);
		break;
	}
	case KSERVICE:
	{
		// SERVICE
		cmd = parse_service(self);
		break;
	}
	case KREBALANCE:
	{
		// REBALANCE
		cmd = parse_rebalance(self);
		break;
	}
	case KCREATE:
	{
		// CREATE CLOUD | STORAGE | PARTITION
		if (lex_if(self, KCLOUD, NULL))
			cmd = parse_cloud_create(self);
		else
		if (lex_if(self, KSTORAGE, NULL))
			cmd = parse_storage_create(self);
		else
		if (lex_if(self, KPARTITION, NULL))
			cmd = parse_partition_create(self);
		else
			error("CREATE 'CLOUD|STORAGE|PARTITION' expected");
		break;
	}
	case KDROP:
	{
		// DROP CLOUD | STORAGE | PARTITION | PARTITIONS
		if (lex_if(self, KCLOUD, NULL))
			cmd = parse_cloud_drop(self);
		else
		if (lex_if(self, KSTORAGE, NULL))
			cmd = parse_storage_drop(self);
		else
		if (lex_if(self, KPARTITION, NULL))
			cmd = parse_partition_drop(self);
		else
		if (lex_if(self, KPARTITIONS, NULL))
			cmd = parse_partition_drop_range(self);
		else
			error("DROP 'STORAGE|PARTITION|PARTITIONS' expected");
		break;
	}
	case KALTER:
	{
		// ALTER CLOUD | STORAGE | PIPELINE
		if (lex_if(self, KCLOUD, NULL))
			cmd = parse_cloud_alter(self);
		else
		if (lex_if(self, KSTORAGE, NULL))
			cmd = parse_storage_alter(self);
		else
		if (lex_if(self, KPIPELINE, NULL))
			cmd = parse_pipeline_alter(self);
		else
			error("ALTER 'STORAGE|PIPELINE' expected");
		break;
	}
	case KMOVE:
	{

		// MOVE PARTITION | PARTITIONS
		if (lex_if(self, KPARTITION, NULL))
			cmd = parse_partition_move(self);
		else
		if (lex_if(self, KPARTITIONS, NULL))
			cmd = parse_partition_move_range(self);
		else
			error("MOVE 'PARTITION|PARTITIONS' expected");
		break;
	}
	case KREFRESH:
	{
		// REFRESH PARTITION | PARTITIONS
		if (lex_if(self, KPARTITION, NULL))
			cmd = parse_partition_refresh(self);
		else
		if (lex_if(self, KPARTITIONS, NULL))
			cmd = parse_partition_refresh_range(self);
		else
			error("REFRESH 'PARTITION|PARTITIONS' expected");
		break;
	}
	case KDOWNLOAD:
	{
		// DOWNLOAD PARTITION | PARTITIONS
		if (lex_if(self, KPARTITION, NULL))
			cmd = parse_partition_download(self);
		else
		if (lex_if(self, KPARTITIONS, NULL))
			cmd = parse_partition_download_range(self);
		else
			error("DOWNLOAD 'PARTITION|PARTITIONS' expected");
		break;
	}
	case KUPLOAD:
	{
		// UPLOAD PARTITION | PARTITIONS
		if (lex_if(self, KPARTITION, NULL))
			cmd = parse_partition_upload(self);
		else
		if (lex_if(self, KPARTITIONS, NULL))
			cmd = parse_partition_upload_range(self);
		else
			error("UPLOAD 'PARTITION|PARTITIONS' expected");
		break;
	}
	case KEOF:
		break;
	default:
		error("unknown command");
		break;
	}

	// eof
	if (! lex_if(self, KEOF, NULL))
	{
		if (cmd)
			cmd_free(cmd);
		error("unexpected clause at the end of command");
	}

	return cmd;
}
