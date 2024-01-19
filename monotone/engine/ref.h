#pragma once

//
// monotone
//
// time-series storage
//

typedef struct Ref Ref;

typedef enum
{
	LOCK_SERVICE,
	LOCK_ACCESS
} RefLock;

struct Ref
{
	bool  refresh;
	Part* part;
	Lock  lock_service;
	Lock  lock_access;
	Slice slice;
};

static inline Ref*
ref_allocate(uint64_t min, uint64_t max)
{
	auto self = (Ref*)mn_malloc(sizeof(Ref));
	self->refresh = false;
	self->part    = NULL;
	lock_init(&self->lock_service);
	lock_init(&self->lock_access);
	slice_init(&self->slice, min, max);
	return self;
}

static inline void
ref_free(Ref* self)
{
	if (self->part)
		part_free(self->part);
	mn_free(self);
}

static inline void
ref_prepare(Ref* self, Mutex* mutex, CondVar* cond_var, Part* part)
{
	self->part = part;
	lock_prepare(&self->lock_access, mutex, cond_var);
	lock_prepare(&self->lock_service, mutex, cond_var);
}

always_inline static inline Ref*
ref_of(Slice* slice)
{
	return container_of(slice, Ref, slice);
}

static inline Lock*
ref_lockof(Ref* self, RefLock lock)
{
	Lock* ptr;
	switch (lock) {
	case LOCK_SERVICE:
		ptr = &self->lock_service;
		break;
	case LOCK_ACCESS:
		ptr = &self->lock_access;
		break;
	}
	return ptr;
}

static inline void
ref_lock(Ref* self, RefLock type)
{
	auto lock = ref_lockof(self, type);
	lock_lock(lock);
}

static inline void
ref_unlock(Ref* self, RefLock type)
{
	auto lock = ref_lockof(self, type);
	lock_unlock(lock);
}
