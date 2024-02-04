#pragma once

//
// monotone
//
// time-series partition
//

typedef struct CmdPartition CmdPartition;

struct CmdPartition
{
	Cmd      cmd;
	uint64_t min;
	uint64_t max;
	bool     if_exists;
	bool     if_cloud;
	int      mask;
	Token    storage;
};

static inline CmdPartition*
cmd_partition_of(Cmd* self)
{
	return (CmdPartition*)self;
}

static inline CmdPartition*
cmd_partition_allocate(CmdType type)
{
	CmdPartition* self;
	self = cmd_allocate(type, NULL, sizeof(*self));
	self->min       = 0;
	self->max       = 0;
	self->if_exists = false;
	self->if_cloud  = false;
	self->mask      = PART_FILE_NONE;
	token_init(&self->storage);
	return self;
}

Cmd* parse_partition_drop(Lex*);
Cmd* parse_partition_drop_range(Lex*);
Cmd* parse_partition_move(Lex*);
Cmd* parse_partition_move_range(Lex*);
Cmd* parse_partition_refresh(Lex*);
Cmd* parse_partition_refresh_range(Lex*);
Cmd* parse_partition_download(Lex*);
Cmd* parse_partition_download_range(Lex*);
Cmd* parse_partition_upload(Lex*);
Cmd* parse_partition_upload_range(Lex*);
