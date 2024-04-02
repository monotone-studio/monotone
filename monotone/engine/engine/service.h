#pragma once

//
// monotone
//
// time-series storage
//

typedef struct Service Service;

typedef enum
{
	SERVICE_ANY,
	SERVICE_WITHOUT_UPLOAD,
	SERVICE_UPLOAD
} ServiceFilter;

struct Service
{
	Mutex   lock;
	CondVar cond_var;
	List    list;
	int     list_count;
	int     list_count_upload;
	bool    shutdown;
};

static inline void
service_init(Service* self)
{
	self->shutdown          = false;
	self->list_count        = 0;
	self->list_count_upload = 0;
	list_init(&self->list);
	cond_var_init(&self->cond_var);
	mutex_init(&self->lock);
}

static inline void
service_free(Service* self)
{
	list_foreach_safe(&self->list)
	{
		auto req = list_at(ServiceReq, link);
		service_req_free(req);
	}
	cond_var_free(&self->cond_var);
	mutex_free(&self->lock);
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
service_schedule(Service* self, ServiceReq* req)
{
	mutex_lock(&self->lock);
	list_append(&self->list, &req->link);
	self->list_count++;
	bool is_upload = service_req_is_upload(req);
	if (is_upload)
		self->list_count_upload++;
	cond_var_broadcast(&self->cond_var);
	mutex_unlock(&self->lock);
}

static inline void
service_add(Service* self, uint64_t id, int count, ...)
{
	// allocate request
	assert(count > 0);
	va_list args;
	va_start(args, count);
	auto req = service_req_allocate(id, count, args);
	va_end(args);

	// schedule request execution based on the
	// first action type
	service_schedule(self, req);
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

static inline ServiceReq*
service_fetch(Service* self, ServiceFilter filter)
{
	if (self->list_count == 0)
		return NULL;

	ServiceReq* req = NULL;
	switch (filter) {
	case SERVICE_ANY:
	{
		req = container_of(list_pop(&self->list), ServiceReq, link);
		self->list_count--;
		if (service_req_is_upload(req))
			self->list_count_upload--;
		break;
	}
	case SERVICE_WITHOUT_UPLOAD:
	{
		if ((self->list_count - self->list_count_upload) == 0)
			break;
		list_foreach_safe(&self->list)
		{
			auto at = list_at(ServiceReq, link);
			if (service_req_is_upload(at))
				continue;
			list_unlink(&at->link);
			self->list_count--;
			req = at;
			break;
		}
		break;
	}
	case SERVICE_UPLOAD:
	{
		if (self->list_count_upload == 0)
			break;

		list_foreach_safe(&self->list)
		{
			auto at = list_at(ServiceReq, link);
			if (! service_req_is_upload(at))
				continue;
			list_unlink(&at->link);
			self->list_count--;
			self->list_count_upload--;
			req = at;
			break;
		}
		break;
	}
	}

	return req;
}

static inline bool
service_next(Service*      self,
             ServiceFilter filter, bool wait,
             ServiceReq**  req)
{
	mutex_lock(&self->lock);

	auto shutdown = false;
	for (;;)
	{
		if (unlikely(self->shutdown))
		{
			shutdown = true;
			break;
		}
		*req = service_fetch(self, filter);
		if (*req || !wait)
			break;

		cond_var_wait(&self->cond_var, &self->lock);
	}

	mutex_unlock(&self->lock);
	return shutdown;
}

static inline bool
service_filter(ServiceReq* req, ServiceFilter filter)
{
	bool pass;
	switch (filter) {
	case SERVICE_ANY:
		pass = true;
		break;
	case SERVICE_WITHOUT_UPLOAD:
		pass = !service_req_is_upload(req);
		break;
	case SERVICE_UPLOAD:
		pass = service_req_is_upload(req);
		break;
	}
	return pass;
}
