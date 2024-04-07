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
