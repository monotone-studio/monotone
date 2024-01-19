#pragma once

//
// monotone
//
// time-series storage
//

typedef struct Lock Lock;

struct Lock
{
	pthread_t locker;
	int       locker_refs;
	int       pending;
	Mutex*    mutex;
	CondVar*  cond_var;
};

static inline void
lock_init(Lock* self)
{
	self->locker      = 0;
	self->locker_refs = 0;
	self->pending     = 0;
	self->mutex       = NULL;
	self->cond_var    = NULL;
}

static inline void
lock_prepare(Lock* self, Mutex* mutex, CondVar* cond_var)
{
	self->mutex    = mutex;
	self->cond_var = cond_var;
}

static inline void
lock_lock(Lock* self)
{
	// lock mutex must be locked before this call

	// nested call
	pthread_t pthread_id = pthread_self();
	if (pthread_equal(pthread_id, self->locker))
	{
		self->locker_refs++;
		return;
	}

	// wait for lock
	for (;;)
	{
		if (self->locker == 0)
		{
			self->locker = pthread_id;
			self->locker_refs = 0;
			break;
		}
		self->pending++;
		cond_var_wait(self->cond_var, self->mutex);
		self->pending--;
	}
}

static inline void
lock_unlock(Lock* self)
{
	// lock mutex must be locked before this call
	self->locker_refs--;
	if (self->locker_refs > 0)
		return;

	// wakeup waiter
	self->locker = 0;
	self->locker_refs = 0;
	if (self->pending > 0)
	{
		// using broadcast since cond var can be shared between
		// other lock types
		cond_var_broadcast(self->cond_var);
		return;
	}
}
