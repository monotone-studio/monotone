#pragma once

//
// monotone
//
// time-series storage
//

typedef struct CmdService CmdService;

struct CmdService
{
	Cmd cmd;
};

static inline CmdService*
cmd_service_of(Cmd* self)
{
	return (CmdService*)self;
}

static inline CmdService*
cmd_service_allocate(CmdType type)
{
	CmdService* self;
	self = cmd_allocate(type, NULL, sizeof(*self));
	return self;
}

static inline Cmd*
parse_checkpoint(Lex* self)
{
	unused(self);
	return &cmd_service_allocate(CMD_CHECKPOINT)->cmd;
}

static inline Cmd*
parse_service(Lex* self)
{
	unused(self);
	return &cmd_service_allocate(CMD_SERVICE)->cmd;
}

static inline Cmd*
parse_rebalance(Lex* self)
{
	unused(self);
	return &cmd_service_allocate(CMD_REBALANCE)->cmd;
}
