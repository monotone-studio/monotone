#pragma once

//
// monotone
//
// time-series pipeline
//

typedef struct CmdPipelineAlter CmdPipelineAlter;

struct CmdPipelineAlter
{
	Cmd  cmd;
	List list;
	int  list_count;
};

static inline CmdPipelineAlter*
cmd_pipeline_alter_of(Cmd* self)
{
	return (CmdPipelineAlter*)self;
}

static inline void
cmd_pipeline_alter_free(Cmd* self)
{
	auto cmd = cmd_pipeline_alter_of(self);
	list_foreach_safe(&cmd->list)
	{
		auto config = list_at(TierConfig, link);
		tier_config_free(config);
	}
}

static inline CmdPipelineAlter*
cmd_pipeline_alter_allocate(void)
{
	CmdPipelineAlter* self;
	self = cmd_allocate(CMD_PIPELINE_ALTER, cmd_pipeline_alter_free,
	                    sizeof(*self));
	self->list_count = 0;
	list_init(&self->list);
	return self;
}

static inline void
cmd_pipeline_alter_add(CmdPipelineAlter* self, TierConfig* config)
{
	list_append(&self->list, &config->link);
	self->list_count++;
}

Cmd* parse_pipeline_alter(Lex*);
