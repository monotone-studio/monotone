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

typedef struct CmdShowOp CmdShowOp;
typedef struct CmdShow   CmdShow;
typedef struct CmdSet    CmdSet;

enum
{
	SHOW_MEMORY,
	SHOW_WAL,
	SHOW_CLOUDS,
	SHOW_STORAGES,
	SHOW_PARTITION,
	SHOW_PARTITIONS,
	SHOW_PIPELINE,
	SHOW_CONFIG,
	SHOW_NAME
};

struct CmdShowOp
{
	int   type;
	bool  debug;
	Token name;
	List  link;
};

struct CmdShow
{
	Cmd  cmd;
	List list;
	int  list_count;
};

struct CmdSet
{
	Cmd   cmd;
	Token name;
	Token value;
};

static inline CmdShow*
cmd_show_of(Cmd* self)
{
	return (CmdShow*)self;
}

static inline CmdSet*
cmd_set_of(Cmd* self)
{
	return (CmdSet*)self;
}

static inline void
cmd_show_free(Cmd* self)
{
	auto cmd = cmd_show_of(self);
	list_foreach_safe(&cmd->list)
	{
		auto op = list_at(CmdShowOp, link);
		mn_free(op);
	}
}

static inline CmdShowOp*
cmd_show_allocate_op(int type, Token* name, bool debug)
{
	CmdShowOp* self = mn_malloc(sizeof(CmdShowOp));
	self->type  = type;
	self->debug = debug;
	self->name  = *name;
	list_init(&self->link);
	return self;
}

static inline CmdShow*
cmd_show_allocate(void)
{
	CmdShow* self;
	self = cmd_allocate(CMD_SHOW, cmd_show_free, sizeof(*self));
	self->list_count = 0;
	list_init(&self->list);
	return self;
}

static inline CmdSet*
cmd_set_allocate(Token* name, Token* value)
{
	CmdSet* self;
	self = cmd_allocate(CMD_SET, NULL, sizeof(*self));
	self->name  = *name;
	self->value = *value;
	return self;
}

Cmd* parse_show(Lex*);
Cmd* parse_set(Lex*);
