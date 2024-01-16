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
		auto part = list_at(ServicePart, link);
		service_part_free(part);
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

hot static inline ServicePart*
service_find(Service* self, uint64_t min)
{
	list_foreach(&self->list)
	{
		auto part = list_at(ServicePart, link);
		if (part->min == min)
			return part;
	}
	return NULL;
}

hot static inline ServicePart*
service_find_or_create(Service* self, uint64_t min)
{
	auto part = service_find(self, min);
	if (part == NULL)
	{
		part = service_part_allocate(min);
		list_append(&self->list, &part->link);
		self->list_count++;
	}
	return part;
}

static inline void
service_add(Service* self, ServiceType type, uint64_t min, Str* storage)
{
	auto req = service_req_allocate(type, storage);

	mutex_lock(&self->lock);
	guard(guard, mutex_unlock, &self->lock);

	auto part = service_find_or_create(self, min);
	service_part_add(part, req);
	cond_var_signal(&self->cond_var);
}

hot static inline void
service_add_if_not_pending(Service* self, ServiceType type, uint64_t min, Str* storage)
{
	mutex_lock(&self->lock);
	guard(guard, mutex_unlock, &self->lock);

	auto part = service_find(self, min);
	if (part)
		return;

	part = service_part_allocate(min);
	guard(guard_free, service_part_free, part);

	auto req = service_req_allocate(type, storage);
	service_part_add(part, req);
	list_append(&self->list, &part->link);
	self->list_count++;

	cond_var_signal(&self->cond_var);
	unguard(&guard_free);
}

static inline ServicePart*
service_next_match(Service* self)
{
	list_foreach(&self->list)
	{
		auto part = list_at(ServicePart, link);
		if (part->list_count > 0 && !part->active)
		{
			part->active = true;
			return part;
		}
	}
	return NULL;
}

static inline ServicePart*
service_next(Service* self)
{
	mutex_lock(&self->lock);

	ServicePart* part = NULL;
	while (! self->shutdown)
	{
		part = service_next_match(self);
		if (part)
			break;
		cond_var_wait(&self->cond_var, &self->lock);
	}

	mutex_unlock(&self->lock);
	return part;
}

static inline void
service_complete(Service* self, ServicePart* part)
{
	bool free = false;
	mutex_lock(&self->lock);

	service_part_pop(part);
	part->active = false;
	if (part->list_count == 0)
	{
		free = true;
		list_unlink(&part->link);
		self->list_count--;
	} else {
		cond_var_signal(&self->cond_var);
	}
	mutex_unlock(&self->lock);

	if (free)
		service_part_free(part);
}
