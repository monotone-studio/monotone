#pragma once

//
// monotone
//
// time-series storage
//

typedef struct ServicePart ServicePart;

struct ServicePart
{
	bool     active;
	uint64_t min;
	List     list;
	int      list_count;
	List     link;
};

static inline ServicePart*
service_part_allocate(uint64_t min)
{
	auto self = (ServicePart*)mn_malloc(sizeof(ServicePart));
	self->active     = false;
	self->min        = min;
	self->list_count = 0;
	list_init(&self->list);
	list_init(&self->link);
	return self;
}

static inline void
service_part_free(ServicePart* self)
{
	list_foreach_safe(&self->list)
	{
		auto req = list_at(ServiceReq, link);
		service_req_free(req);
	}
	mn_free(self);
}

static inline void
service_part_add(ServicePart* self, ServiceReq* req)
{
	list_append(&self->list, &req->link);
	self->list_count++;
}

static inline void
service_part_pop(ServicePart* self)
{
	if (self->list_count == 0)
		return;
	auto first = list_pop(&self->list);
	self->list_count--;
	auto req = container_of(first, ServiceReq, link);
	service_req_free(req);
}
