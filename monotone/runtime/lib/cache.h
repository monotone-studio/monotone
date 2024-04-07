#pragma once

//
// monotone.
//
// embeddable cloud-native storage for events
// and time-series data.
//
// Copyright (c) 2023-2024 Dmitry Simonenko
// Copyright (c) 2023-2024 Monotone
//
// Distributed under the terms of GNU LGPL v3 or
// Monotone Commercial License.
//

typedef struct Cache Cache;

struct Cache
{
	Spinlock lock;
	List     list;
	int      list_count;
};

static inline void
cache_init(Cache* self)
{
	self->list_count = 0;
	list_init(&self->list);
	spinlock_init(&self->lock);
}

static inline void
cache_free(Cache* self)
{
	assert(! self->list_count);
	spinlock_free(&self->lock);
}

static inline void
cache_reset(Cache* self)
{
	spinlock_lock(&self->lock);
	list_init(&self->list);
	self->list_count = 0;
	spinlock_unlock(&self->lock);
}

static inline List*
cache_pop(Cache* self)
{
	spinlock_lock(&self->lock);
	if (likely(self->list_count > 0))
	{
		auto obj = list_pop(&self->list);
		self->list_count--;
		spinlock_unlock(&self->lock);
		return obj;
	}
	spinlock_unlock(&self->lock);
	return NULL;
}

static inline void
cache_push(Cache* self, List* obj)
{
	spinlock_lock(&self->lock);
	list_append(&self->list, obj);
	self->list_count++;
	spinlock_unlock(&self->lock);
}
