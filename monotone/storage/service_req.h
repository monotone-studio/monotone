#pragma once

//
// monotone
//
// time-series storage
//

typedef struct ServiceReq ServiceReq;

typedef enum
{
	SERVICE_MERGE,
	SERVICE_MOVE,
	SERVICE_DROP
} ServiceType;

struct ServiceReq
{
	ServiceType type;
	Str         storage;
	List        link;
};

static inline void
service_req_free(ServiceReq* self)
{
	str_free(&self->storage);
	mn_free(self);
}

static inline ServiceReq*
service_req_allocate(ServiceType type, Str* storage)
{
	auto self = (ServiceReq*)mn_malloc(sizeof(ServiceReq));
	self->type = type;
	str_init(&self->storage);
	list_init(&self->link);
	guard(guard, service_req_free, self);
	if (storage)
		str_copy(&self->storage, storage);
	return unguard(&guard);
}
