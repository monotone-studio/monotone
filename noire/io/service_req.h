#pragma once

//
// noire
//
// time-series storage
//

typedef struct ServiceReq ServiceReq;

struct ServiceReq
{
	uint64_t min;
	uint64_t max;
	List     link;
};

static inline ServiceReq*
service_req_allocate(uint64_t min, uint64_t max)
{
	auto self = (ServiceReq*)nr_malloc(sizeof(ServiceReq));
	self->min = min;
	self->max = max;
	list_init(&self->link);
	return self;
}

static inline void
service_req_free(ServiceReq* self)
{
	nr_free(self);
}
