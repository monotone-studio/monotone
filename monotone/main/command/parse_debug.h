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

typedef struct CmdDebug CmdDebug;

enum
{
	DEBUG_WAL_CREATE,
	DEBUG_WAL_GC,
	DEBUG_WAL_READ,
	DEBUG_MEMORY_GC
};

struct CmdDebug
{
	Cmd cmd;
	int command;
};

static inline CmdDebug*
cmd_debug_of(Cmd* self)
{
	return (CmdDebug*)self;
}

static inline CmdDebug*
cmd_debug_allocate(void)
{
	CmdDebug* self;
	self = cmd_allocate(CMD_DEBUG, NULL, sizeof(*self));
	self->command = 0;
	return self;
}

Cmd* parse_debug(Lex*);
