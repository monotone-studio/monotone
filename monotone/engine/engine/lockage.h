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

typedef struct Lock    Lock;
typedef struct Lockage Lockage;

typedef enum
{
	LOCK_SERVICE,
	LOCK_ACCESS,
	LOCK_MAX
} LockType;

struct Lock
{
	pthread_t locker;
	int       locker_refs;
	int       pending;
};

struct Lockage
{
	Lock     locks[LOCK_MAX];
	Mutex*   mutex;
	CondVar* cond_var;
};

static inline void
lockage_init(Lockage* self)
{
	LockType type = LOCK_SERVICE;
	for (; type < LOCK_MAX; type++)
	{
		auto lock = &self->locks[type];
		lock->locker      = 0;
		lock->locker_refs = 0;
		lock->pending     = 0;
	}
	self->mutex    = NULL;
	self->cond_var = NULL;
}

static inline void
lockage_set(Lockage* self, Mutex* mutex, CondVar* cond_var)
{
	self->mutex    = mutex;
	self->cond_var = cond_var;
}

static inline void
lockage_lock(Lockage* self, LockType type)
{
	// mutex must be always taken before this call
	auto lock = &self->locks[type];

	// nested call
	pthread_t pthread_id = pthread_self();
	if (pthread_equal(pthread_id, lock->locker))
	{
		lock->locker_refs++;
		return;
	}

	// wait for lock
	for (;;)
	{
		if (lock->locker == 0)
		{
			lock->locker = pthread_id;
			lock->locker_refs = 0;
			break;
		}
		lock->pending++;
		cond_var_wait(self->cond_var, self->mutex);
		lock->pending--;
	}
}

static inline void
lockage_unlock(Lockage* self, LockType type)
{
	// mutex must be always taken before this call
	auto lock = &self->locks[type];

	lock->locker_refs--;
	if (lock->locker_refs > 0)
		return;

	// wakeup waiter
	lock->locker = 0;
	lock->locker_refs = 0;
	if (lock->pending > 0)
	{
		// using broadcast since cond var can be shared between
		// other lock types
		cond_var_broadcast(self->cond_var);
		return;
	}
}
