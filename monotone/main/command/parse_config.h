#pragma once

//
// monotone
//
// time-series storage
//

typedef struct CmdShow CmdShow;
typedef struct CmdSet  CmdSet;

enum
{
	SHOW_WAL,
	SHOW_STORAGES,
	SHOW_PARTITIONS,
	SHOW_CONVEYOR,
	SHOW_ALL,
	SHOW_NAME
};

struct CmdShow
{
	Cmd   cmd;
	int   type;
	bool  verbose;
	bool  debug;
	Token name;
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

static inline CmdShow*
cmd_show_allocate(int type, Token* name, bool verbose, bool debug)
{
	CmdShow* self;
	self = cmd_allocate(CMD_SHOW, NULL, sizeof(*self));
	self->type    = type;
	self->name    = *name;
	self->verbose = verbose;
	self->debug   = debug;
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
