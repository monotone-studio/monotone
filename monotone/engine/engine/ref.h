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

typedef struct Ref Ref;

struct Ref
{
	Part*   part;
	Lockage lockage;
	Slice   slice;
};

static inline Ref*
ref_allocate(uint64_t min, uint64_t max)
{
	auto self = (Ref*)mn_malloc(sizeof(Ref));
	self->part = NULL;
	lockage_init(&self->lockage);
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
	lockage_set(&self->lockage, mutex, cond_var);
}

always_inline static inline Ref*
ref_of(Slice* slice)
{
	return container_of(slice, Ref, slice);
}

static inline void
ref_lock(Ref* self, LockType type)
{
	lockage_lock(&self->lockage, type);
}

static inline void
ref_unlock(Ref* self, LockType type)
{
	lockage_unlock(&self->lockage, type);
}
