#pragma once

//
// monotone
//
// time-series storage
//

typedef struct CmdShow CmdShow;
typedef struct CmdSet  CmdSet;

struct CmdShow
{
	Cmd   cmd;
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
cmd_show_allocate(Token* name)
{
	CmdShow* self;
	self = cmd_allocate(CMD_SHOW, NULL, sizeof(*self));
	self->name = *name;
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
