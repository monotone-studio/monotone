#pragma once

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
	self->mask      = ID_NONE;
	token_init(&self->storage);
	return self;
}

Cmd* parse_partition_create(Lex*);
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
