#pragma once

//
// monotone
//
// time-series storage
//

typedef struct Ref Ref;

enum
{
	LOCK_SERVICE = 1,
	LOCK_ACCESS  = 2
};

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

static inline void
ref_lock(Ref* self, int lock)
{
	if ((lock & LOCK_SERVICE) > 0)
		lock_lock(&self->lock_service);

	if ((lock & LOCK_ACCESS) > 0)
		lock_lock(&self->lock_access);
}

static inline void
ref_unlock(Ref* self, int lock)
{
	if ((lock & LOCK_SERVICE) > 0)
		lock_unlock(&self->lock_service);

	if ((lock & LOCK_ACCESS) > 0)
		lock_unlock(&self->lock_access);
}
