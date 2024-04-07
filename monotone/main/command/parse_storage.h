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

typedef struct CmdStorageCreate CmdStorageCreate;
typedef struct CmdStorageDrop   CmdStorageDrop;
typedef struct CmdStorageAlter  CmdStorageAlter;

struct CmdStorageCreate
{
	Cmd     cmd;
	bool    if_not_exists;
	Source* config;
};

struct CmdStorageDrop
{
	Cmd   cmd;
	bool  if_exists;
	Token name;
};

struct CmdStorageAlter
{
	Cmd     cmd;
	bool    if_exists;
	Token   name;
	Token   name_new;
	Source* config;
	int     config_mask;
};

static inline CmdStorageCreate*
cmd_storage_create_of(Cmd* self)
{
	return (CmdStorageCreate*)self;
}

static inline CmdStorageDrop*
cmd_storage_drop_of(Cmd* self)
{
	return (CmdStorageDrop*)self;
}

static inline CmdStorageAlter*
cmd_storage_alter_of(Cmd* self)
{
	return (CmdStorageAlter*)self;
}

static inline void
cmd_storage_create_free(Cmd* self)
{
	auto cmd = cmd_storage_create_of(self);
	if (cmd->config)
		source_free(cmd->config);
}

static inline CmdStorageCreate*
cmd_storage_create_allocate(void)
{
	CmdStorageCreate* self;
	self = cmd_allocate(CMD_STORAGE_CREATE, cmd_storage_create_free,
	                    sizeof(*self));
	self->if_not_exists = false;
	self->config        = NULL;
	return self;
}

static inline CmdStorageDrop*
cmd_storage_drop_allocate(void)
{
	CmdStorageDrop* self;
	self = cmd_allocate(CMD_STORAGE_DROP, NULL, sizeof(*self));
	self->if_exists = false;
	token_init(&self->name);
	return self;
}

static inline void
cmd_storage_alter_free(Cmd* self)
{
	auto cmd = cmd_storage_alter_of(self);
	if (cmd->config)
		source_free(cmd->config);
}

static inline CmdStorageAlter*
cmd_storage_alter_allocate(void)
{
	CmdStorageAlter* self;
	self = cmd_allocate(CMD_STORAGE_ALTER, cmd_storage_alter_free,
	                    sizeof(*self));
	self->if_exists   = false;
	self->config      = NULL;
	self->config_mask = 0;
	token_init(&self->name);
	token_init(&self->name_new);
	return self;
}

Cmd* parse_storage_create(Lex*);
Cmd* parse_storage_drop(Lex*);
Cmd* parse_storage_alter(Lex*);
