#pragma once

//
// monotone
//
// time-series storage
//

typedef struct Cmd Cmd;

typedef void (*CmdFree)(Cmd*);

typedef enum
{
	CMD_UNDEF,
	CMD_SHOW,
	CMD_SET,
	CMD_CHECKPOINT,
	CMD_REBALANCE,
	CMD_STORAGE_CREATE,
	CMD_STORAGE_DROP,
	CMD_STORAGE_ALTER,
	CMD_CONVEYOR_ALTER,
	CMD_PARTITION_DROP,
	CMD_PARTITION_DROP_RANGE,
	CMD_PARTITION_MOVE,
	CMD_PARTITION_MOVE_RANGE,
	CMD_PARTITION_REFRESH,
	CMD_PARTITION_REFRESH_RANGE,
	CMD_MAX
} CmdType;

struct Cmd
{
	CmdType type;
	CmdFree free;
};

static inline void*
cmd_allocate(CmdType type, CmdFree free, size_t size)
{
	assert(size >= sizeof(Cmd));
	Cmd* self = mn_malloc(size);
	self->type = type; 
	self->free = free;
	return self;
}

static inline void
cmd_free(Cmd* self)
{
	if (self->free)
		self->free(self);
	mn_free(self);
}
