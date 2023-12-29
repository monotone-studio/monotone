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
	uint64_t max;
	List     link;
};

static inline void
service_req_init(ServiceReq* self)
{
	self->min = 0;
	self->max = 0;
	list_init(&self->link);
}

static inline ServiceReq*
service_req_allocate(int64_t min, uint64_t max)
{
	auto self = (ServiceReq*)nr_malloc(sizeof(ServiceReq));
	service_req_init(self);
	self->min = min;
	self->max = max;
	return self;
}

static inline void
service_req_free(ServiceReq* self)
{
	nr_free(self);
}

static inline bool
service_req_rebalance(ServiceReq* self)
{
	return self->min == 0 && self->max == 0;
}
