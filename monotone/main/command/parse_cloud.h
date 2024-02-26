#pragma once

//
// monotone
//
// time-series storage
//

typedef struct CmdCloudCreate CmdCloudCreate;
typedef struct CmdCloudDrop   CmdCloudDrop;
typedef struct CmdCloudAlter  CmdCloudAlter;

struct CmdCloudCreate
{
	Cmd          cmd;
	bool         if_not_exists;
	CloudConfig* config;
};

struct CmdCloudDrop
{
	Cmd   cmd;
	bool  if_exists;
	Token name;
};

struct CmdCloudAlter
{
	Cmd          cmd;
	bool         if_exists;
	Token        name;
	Token        name_new;
	CloudConfig* config;
	int          config_mask;
};

static inline CmdCloudCreate*
cmd_cloud_create_of(Cmd* self)
{
	return (CmdCloudCreate*)self;
}

static inline CmdCloudDrop*
cmd_cloud_drop_of(Cmd* self)
{
	return (CmdCloudDrop*)self;
}

static inline CmdCloudAlter*
cmd_cloud_alter_of(Cmd* self)
{
	return (CmdCloudAlter*)self;
}

static inline void
cmd_cloud_create_free(Cmd* self)
{
	auto cmd = cmd_cloud_create_of(self);
	if (cmd->config)
		cloud_config_free(cmd->config);
}

static inline CmdCloudCreate*
cmd_cloud_create_allocate(void)
{
	CmdCloudCreate* self;
	self = cmd_allocate(CMD_CLOUD_CREATE, cmd_cloud_create_free,
	                    sizeof(*self));
	self->if_not_exists = false;
	self->config        = NULL;
	return self;
}

static inline CmdCloudDrop*
cmd_cloud_drop_allocate(void)
{
	CmdCloudDrop* self;
	self = cmd_allocate(CMD_CLOUD_DROP, NULL, sizeof(*self));
	self->if_exists = false;
	token_init(&self->name);
	return self;
}

static inline void
cmd_cloud_alter_free(Cmd* self)
{
	auto cmd = cmd_cloud_alter_of(self);
	if (cmd->config)
		cloud_config_free(cmd->config);
}

static inline CmdCloudAlter*
cmd_cloud_alter_allocate(void)
{
	CmdCloudAlter* self;
	self = cmd_allocate(CMD_CLOUD_ALTER, cmd_cloud_alter_free,
	                    sizeof(*self));
	self->if_exists   = false;
	self->config      = NULL;
	self->config_mask = 0;
	token_init(&self->name);
	token_init(&self->name_new);
	return self;
}

Cmd* parse_cloud_create(Lex*);
Cmd* parse_cloud_drop(Lex*);
Cmd* parse_cloud_alter(Lex*);
