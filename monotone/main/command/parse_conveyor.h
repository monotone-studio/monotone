#pragma once

//
// monotone
//
// time-series conveyor
//

typedef struct CmdConveyorAlter CmdConveyorAlter;

struct CmdConveyorAlter
{
	Cmd  cmd;
	List list;
	int  list_count;
};

static inline CmdConveyorAlter*
cmd_conveyor_alter_of(Cmd* self)
{
	return (CmdConveyorAlter*)self;
}

static inline void
cmd_conveyor_alter_free(Cmd* self)
{
	auto cmd = cmd_conveyor_alter_of(self);
	list_foreach_safe(&cmd->list)
	{
		auto config = list_at(TierConfig, link);
		tier_config_free(config);
	}
}

static inline CmdConveyorAlter*
cmd_conveyor_alter_allocate(void)
{
	CmdConveyorAlter* self;
	self = cmd_allocate(CMD_CONVEYOR_ALTER, cmd_conveyor_alter_free,
	                    sizeof(*self));
	self->list_count = 0;
	list_init(&self->list);
	return self;
}

static inline void
cmd_conveyor_alter_add(CmdConveyorAlter* self, TierConfig* config)
{
	list_append(&self->list, &config->link);
	self->list_count++;
}

Cmd* parse_conveyor_alter(Lex*);
