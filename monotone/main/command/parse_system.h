#pragma once

//
// monotone
//
// time-series storage
//

typedef struct CmdCheckpoint CmdCheckpoint;
typedef struct CmdRebalance  CmdRebalance;

struct CmdCheckpoint
{
	Cmd cmd;
};

struct CmdRebalance
{
	Cmd cmd;
};

static inline CmdCheckpoint*
cmd_checkpoint_of(Cmd* self)
{
	return (CmdCheckpoint*)self;
}

static inline CmdRebalance*
cmd_rebalance_of(Cmd* self)
{
	return (CmdRebalance*)self;
}

static inline CmdCheckpoint*
cmd_checkpoint_allocate(void)
{
	CmdCheckpoint* self;
	self = cmd_allocate(CMD_CHECKPOINT, NULL, sizeof(*self));
	return self;
}

static inline CmdRebalance*
cmd_rebalance_allocate(void)
{
	CmdRebalance* self;
	self = cmd_allocate(CMD_REBALANCE, NULL, sizeof(*self));
	return self;
}

static inline Cmd*
parse_checkpoint(Lex* self)
{
	unused(self);
	return &cmd_checkpoint_allocate()->cmd;
}

static inline Cmd*
parse_rebalance(Lex* self)
{
	unused(self);
	return &cmd_rebalance_allocate()->cmd;
}
