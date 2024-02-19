
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
		// CREATE STORAGE
		if (lex_if(self, KSTORAGE, NULL))
			cmd = parse_storage_create(self);
		else
			error("CREATE <STORAGE> expected");
		break;
	}
	case KDROP:
	{
		// DROP STORAGE | PARTITION | PARTITIONS
		if (lex_if(self, KSTORAGE, NULL))
			cmd = parse_storage_drop(self);
		else
		if (lex_if(self, KPARTITION, NULL))
			cmd = parse_partition_drop(self);
		else
		if (lex_if(self, KPARTITIONS, NULL))
			cmd = parse_partition_drop_range(self);
		else
			error("DROP <STORAGE|PARTITION|PARTITIONS> expected");
		break;
	}
	case KALTER:
	{
		// ALTER STORAGE | CONVEYOR
		if (lex_if(self, KSTORAGE, NULL))
			cmd = parse_storage_alter(self);
		else
		if (lex_if(self, KCONVEYOR, NULL))
			cmd = parse_conveyor_alter(self);
		else
			error("ALTER <STORAGE|CONVEYOR> expected");
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
			error("MOVE <PARTITION|PARTITIONS> expected");
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
			error("REFRESH <PARTITION|PARTITIONS> expected");
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
			error("DOWNLOAD <PARTITION|PARTITIONS> expected");
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
			error("UPLOAD <PARTITION|PARTITIONS> expected");
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
