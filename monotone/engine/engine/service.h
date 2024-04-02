#pragma once

//
// monotone
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
service_add(Service* self, uint64_t id, int count, ...)
{
	va_list args;
	va_start(args, count);
	auto req = service_req_allocate(id, count, args);
	va_end(args);

	mutex_lock(&self->lock);
	list_append(&self->list, &req->link);
	self->list_count++;
	cond_var_signal(&self->cond_var);
	mutex_unlock(&self->lock);
}

static inline void
service_rotate(Service* self)
{
	service_add(self, UINT64_MAX, 2,
	            ACTION_ROTATE,
	            ACTION_GC);
}

static inline void
service_rebalance(Service* self)
{
	service_add(self, UINT64_MAX, 2,
	            ACTION_REBALANCE,
	            ACTION_GC);
}

static inline void
service_refresh(Service* self, Part* part)
{
	if (! part->cloud)
	{
		service_add(self, part->id.min, 3,
		            ACTION_REBALANCE,
		            ACTION_REFRESH,
		            ACTION_GC);
		return;
	}
	service_add(self, part->id.min, 7,
	            ACTION_REBALANCE,
	            ACTION_DOWNLOAD,
	            ACTION_DROP_ON_CLOUD,
	            ACTION_REFRESH,
	            ACTION_UPLOAD,
	            ACTION_DROP_ON_STORAGE,
	            ACTION_GC);
}

static inline bool
service_next(Service* self, bool wait, ServiceReq** req)
{
	mutex_lock(&self->lock);

	bool shutdown = false;
	for (;;)
	{
		if (unlikely(self->shutdown))
		{
			shutdown = true;
			break;
		}

		if (self->list_count > 0)
		{
			*req = container_of(list_pop(&self->list), ServiceReq, link);
			self->list_count--;
			break;
		}

		if (! wait)
			break;

		cond_var_wait(&self->cond_var, &self->lock);
	}

	mutex_unlock(&self->lock);
	return shutdown;
}
