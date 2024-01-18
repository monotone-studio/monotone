#pragma once

//
// monotone
//
// time-series storage
//

typedef struct Service Service;

typedef enum
{
	SERVICE_SHUTDOWN,
	SERVICE_REBALANCE,
	SERVICE_REFRESH
} ServiceType;

struct Service
{
	Mutex   lock;
	CondVar cond_var;
	List    list;
	int     list_count;
	bool    rebalance;
	bool    shutdown;
};

static inline void
service_init(Service* self)
{
	self->list_count = 0;
	self->shutdown   = false;
	self->rebalance  = false;
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
service_rebalance(Service* self)
{
	mutex_lock(&self->lock);
	self->rebalance = true;
	cond_var_signal(&self->cond_var);
	mutex_unlock(&self->lock);
}

static inline void
service_refresh(Service* self, uint64_t min)
{
	auto req = service_req_allocate(min);
	mutex_lock(&self->lock);
	list_append(&self->list, &req->link);
	self->list_count++;
	cond_var_signal(&self->cond_var);
	mutex_unlock(&self->lock);
}

static inline ServiceType
service_next(Service* self, ServiceReq** req)
{
	mutex_lock(&self->lock);

	ServiceType type;
	for (;;)
	{
		if (unlikely(self->shutdown))
		{
			type = SERVICE_SHUTDOWN;
			break;
		}
		if (self->rebalance)
		{
			self->rebalance = false;
			type = SERVICE_REBALANCE;
			break;
		}
		if (self->list_count > 0)
		{
			*req = container_of(list_pop(&self->list), ServiceReq, link);
			self->list_count--;
			type = SERVICE_REFRESH;
			break;
		}
		cond_var_wait(&self->cond_var, &self->lock);
	}

	mutex_unlock(&self->lock);
	return type;
}
