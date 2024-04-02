#pragma once

//
// monotone
//
// time-series storage
//

typedef struct Action     Action;
typedef struct ServiceReq ServiceReq;

typedef enum
{
	ACTION_NONE,
	ACTION_ROTATE,
	ACTION_GC,
	ACTION_REBALANCE,
	ACTION_DROP_ON_STORAGE,
	ACTION_DROP_ON_CLOUD,
	ACTION_REFRESH,
	ACTION_DOWNLOAD,
	ACTION_UPLOAD
} ActionType;

struct Action
{
	ActionType type;
};

struct ServiceReq
{
	uint64_t id;
	List     link;
	int      current;
	int      actions_count;
	Action   actions[];
};

static inline ServiceReq*
service_req_allocate(uint64_t id, int count, va_list args)
{
	auto self = (ServiceReq*)mn_malloc(sizeof(ServiceReq) + sizeof(Action) * count);
	self->id = id;
	self->current = 0;
	self->actions_count = count;
	list_init(&self->link);
	for (int i = 0; i < count; i++)
		self->actions[i].type = va_arg(args, ActionType);
	return self;
}

static inline void
service_req_free(ServiceReq* self)
{
	mn_free(self);
}

static inline bool
service_req_is_upload(ServiceReq* self)
{
	return self->actions[self->current].type == ACTION_UPLOAD;
}
