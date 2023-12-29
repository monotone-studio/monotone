#pragma once

//
// noire
//
// time-series storage
//

typedef struct Service Service;

struct Service
{
	Mutex   lock;
	CondVar cond_var;
	List    list;
	int     list_count;
	bool    shutdown;
};

static inline void
service_init(Service* self)
{
	self->list_count = 0;
	self->shutdown   = false;
	mutex_init(&self->lock);
	cond_var_init(&self->cond_var);
	list_init(&self->list);
}

static inline void
service_free(Service* self)
{
	list_foreach_safe(&self->list)
	{
		auto req = list_at(ServiceReq, link);
		service_req_free(req);
	}
	mutex_free(&self->lock);
	cond_var_free(&self->cond_var);
}

static inline void
service_shutdown(Service* self)
{
	mutex_lock(&self->lock);
	self->shutdown = true;
	cond_var_broadcast(&self->cond_var);
	mutex_unlock(&self->lock);
}

static inline void
service_add(Service* self, uint64_t min, uint64_t max)
{
	auto req = service_req_allocate(min, max);
	mutex_lock(&self->lock);
	list_append(&self->list, &req->link);
	self->list_count++;
	cond_var_signal(&self->cond_var);
	mutex_unlock(&self->lock);
}

static inline void
service_add_rebalance(Service* self)
{
	service_add(self, 0, 0);
}

static inline ServiceReq*
service_next(Service* self)
{
	ServiceReq* req = NULL;
	mutex_lock(&self->lock);
	while (! self->shutdown)
	{
		if (self->list_count == 0)
		{
			cond_var_wait(&self->cond_var, &self->lock);
			continue;
		}
		req = container_of(list_pop(&self->list), ServiceReq, link);
		self->list_count--;
		break;
	}
	mutex_unlock(&self->lock);
	return req;
}
