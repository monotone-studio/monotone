#pragma once

//
// monotone
//
// time-series storage
//

typedef struct ServiceReq ServiceReq;

struct ServiceReq
{
	uint64_t min;
	List     link;
};

static inline void
service_req_free(ServiceReq* self)
{
	mn_free(self);
}

static inline ServiceReq*
service_req_allocate(uint64_t min)
{
	auto self = (ServiceReq*)mn_malloc(sizeof(ServiceReq));
	self->min = min;
	list_init(&self->link);
	return self;
}
