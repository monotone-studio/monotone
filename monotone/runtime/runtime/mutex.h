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

typedef struct Mutex Mutex;

struct Mutex
{
	pthread_mutex_t lock;
};

static inline void
mutex_init(Mutex* self)
{
	pthread_mutex_init(&self->lock, NULL);
}

static inline void
mutex_free(Mutex* self)
{
	pthread_mutex_destroy(&self->lock);
}

static inline void
mutex_lock(Mutex* self)
{
	pthread_mutex_lock(&self->lock);
}

static inline void
mutex_unlock(Mutex* self)
{
	pthread_mutex_unlock(&self->lock);
}
