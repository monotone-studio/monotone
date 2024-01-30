#pragma once

//
// monotone
//
// time-series partition
//

typedef struct CmdPartitionDrop         CmdPartitionDrop;
typedef struct CmdPartitionDropRange    CmdPartitionDropRange;
typedef struct CmdPartitionMove         CmdPartitionMove;
typedef struct CmdPartitionMoveRange    CmdPartitionMoveRange;
typedef struct CmdPartitionRefresh      CmdPartitionRefresh;
typedef struct CmdPartitionRefreshRange CmdPartitionRefreshRange;

struct CmdPartitionDrop
{
	Cmd      cmd;
	bool     if_exists;
	uint64_t min;
};

struct CmdPartitionDropRange
{
	Cmd      cmd;
	uint64_t min;
	uint64_t max;
};

struct CmdPartitionMove
{
	Cmd      cmd;
	bool     if_exists;
	uint64_t min;
	Token    storage;
};

struct CmdPartitionMoveRange
{
	Cmd      cmd;
	uint64_t min;
	uint64_t max;
	Token    storage;
};

struct CmdPartitionRefresh
{
	Cmd      cmd;
	bool     if_exists;
	uint64_t min;
};

struct CmdPartitionRefreshRange
{
	Cmd      cmd;
	uint64_t min;
	uint64_t max;
};

static inline CmdPartitionDrop*
cmd_partition_drop_of(Cmd* self)
{
	return (CmdPartitionDrop*)self;
}

static inline CmdPartitionDropRange*
cmd_partition_drop_range_of(Cmd* self)
{
	return (CmdPartitionDropRange*)self;
}

static inline CmdPartitionMove*
cmd_partition_move_of(Cmd* self)
{
	return (CmdPartitionMove*)self;
}

static inline CmdPartitionMoveRange*
cmd_partition_move_range_of(Cmd* self)
{
	return (CmdPartitionMoveRange*)self;
}

static inline CmdPartitionRefresh*
cmd_partition_refresh_of(Cmd* self)
{
	return (CmdPartitionRefresh*)self;
}

static inline CmdPartitionRefreshRange*
cmd_partition_refresh_range_of(Cmd* self)
{
	return (CmdPartitionRefreshRange*)self;
}

static inline CmdPartitionDrop*
cmd_partition_drop_allocate(uint64_t min, bool if_exists)
{
	CmdPartitionDrop* self;
	self = cmd_allocate(CMD_PARTITION_DROP, NULL, sizeof(*self));
	self->min       = min;
	self->if_exists = if_exists;
	return self;
}

static inline CmdPartitionDropRange*
cmd_partition_drop_range_allocate(uint64_t min, uint64_t max)
{
	CmdPartitionDropRange* self;
	self = cmd_allocate(CMD_PARTITION_DROP_RANGE, NULL, sizeof(*self));
	self->min = min;
	self->max = max;
	return self;
}

static inline CmdPartitionMove*
cmd_partition_move_allocate(uint64_t min, Token* storage, bool if_exists)
{
	CmdPartitionMove* self;
	self = cmd_allocate(CMD_PARTITION_MOVE, NULL, sizeof(*self));
	self->min       = min;
	self->storage   = *storage;
	self->if_exists = if_exists;
	return self;
}

static inline CmdPartitionMoveRange*
cmd_partition_move_range_allocate(uint64_t min, uint64_t max, Token* storage)
{
	CmdPartitionMoveRange* self;
	self = cmd_allocate(CMD_PARTITION_MOVE_RANGE, NULL, sizeof(*self));
	self->min     = min;
	self->max     = max;
	self->storage = *storage;
	return self;
}

static inline CmdPartitionRefresh*
cmd_partition_refresh_allocate(uint64_t min, bool if_exists)
{
	CmdPartitionRefresh* self;
	self = cmd_allocate(CMD_PARTITION_REFRESH, NULL, sizeof(*self));
	self->min       = min;
	self->if_exists = if_exists;
	return self;
}

static inline CmdPartitionRefreshRange*
cmd_partition_refresh_range_allocate(uint64_t min, uint64_t max)
{
	CmdPartitionRefreshRange* self;
	self = cmd_allocate(CMD_PARTITION_REFRESH_RANGE, NULL, sizeof(*self));
	self->min = min;
	self->max = max;
	return self;
}

Cmd* parse_partition_drop(Lex*);
Cmd* parse_partition_drop_range(Lex*);
Cmd* parse_partition_move(Lex*);
Cmd* parse_partition_move_range(Lex*);
Cmd* parse_partition_refresh(Lex*);
Cmd* parse_partition_refresh_range(Lex*);
