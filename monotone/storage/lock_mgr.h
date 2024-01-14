#pragma once

//
// monotone
//
// time-series storage
//

typedef struct Lock    Lock;
typedef struct LockMgr LockMgr;

struct Lock
{
	pthread_t locker;
	int       locker_refs;
	int       pending;
	uint64_t  id;
	void*     arg;
	CondVar   cond_var;
	LockMgr*  lock_mgr;
	List      link;
};

struct LockMgr
{
	Mutex mutex;
	int   list_count;
	List  list;
	int   list_free_count;
	List  list_free;
};

static inline Lock*
lock_allocate(void)
{
	auto self = (Lock*)mn_malloc(sizeof(Lock));
	self->locker      = 0;
	self->locker_refs = 0;
	self->pending     = 0;
	self->id          = 0;
	self->arg         = NULL;
	self->lock_mgr    = NULL;
	cond_var_init(&self->cond_var);
	list_init(&self->link);
	return self;
}

static inline void
lock_free(Lock* self)
{
	cond_var_free(&self->cond_var);
	mn_free(self);
}

static inline void
lock_mgr_init(LockMgr* self)
{
	self->list_count = 0;
	self->list_free_count = 0;
	list_init(&self->list);
	list_init(&self->list_free);
	mutex_init(&self->mutex);
}

static inline void
lock_mgr_free(LockMgr* self)
{
	assert(! self->list_count);
	list_foreach_safe(&self->list_free)
	{
		auto lock = list_at(Lock, link);
		lock_free(lock);
	}
	mutex_free(&self->mutex);
}

hot static inline Lock*
lock_mgr_find(LockMgr* self, uint64_t id)
{
	list_foreach(&self->list)
	{
		auto lock = list_at(Lock, link);
		if (lock->id == id)
			return lock;
	}
	return NULL;
}

static inline Lock*
lock_mgr_allocate(LockMgr* self, uint64_t id)
{
	Lock* lock;
	if (likely(self->list_free_count > 0))
	{
		lock = container_of(list_pop(&self->list_free), Lock, link);
		self->list_free_count--;
	} else
	{
		lock = lock_allocate();
	}
	lock->id          = id;
	lock->arg         = NULL;
	lock->locker_refs = 0;
	lock->locker      = 0;
	lock->lock_mgr    = self;
	list_init(&lock->link);
	return lock;
}

hot static inline Lock*
lock_mgr_get(LockMgr* self, uint64_t id)
{
	mutex_lock(&self->mutex);
	guard(unlock, mutex_unlock, &self->mutex);

	// find or create lock
	auto lock = lock_mgr_find(self, id);
	if (! lock)
	{
		lock = lock_mgr_allocate(self, id);
		list_append(&self->list, &lock->link);
		self->list_count++;
	}

	// nested call
	pthread_t pthread_id = pthread_self();
	if (pthread_equal(pthread_id, lock->locker))
	{
		lock->locker_refs++;
		return lock;
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
		cond_var_wait(&lock->cond_var, &self->mutex);
		lock->pending--;
	}
	return lock;
}

hot static inline void
lock_mgr_unlock(Lock* lock)
{
	LockMgr* self = lock->lock_mgr;

	mutex_lock(&self->mutex);
	guard(unlock, mutex_unlock, &self->mutex);

	lock->locker_refs--;
	if (lock->locker_refs > 0)
		return;

	// wakeup waiter
	lock->locker = 0;
	lock->locker_refs = 0;
	if (lock->pending > 0) {
		cond_var_signal(&lock->cond_var);
		return;
	}

	// add lock to the free list
	self->list_count--;
	assert(self->list_count >= 0);
	list_unlink(&lock->link);
	list_append(&self->list_free, &lock->link);
	self->list_free_count++;
}
