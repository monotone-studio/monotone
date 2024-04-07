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

typedef struct Iterator Iterator;

typedef bool (*IteratorHas)(Iterator*);
typedef Event* (*IteratorAt)(Iterator*);
typedef void (*IteratorNext)(Iterator*);
typedef void (*IteratorClose)(Iterator*);

struct Iterator
{
	IteratorHas   has;
	IteratorAt    at;
	IteratorNext  next;
	IteratorClose close;
};

static inline bool
iterator_has(Iterator* self)
{
	return self->has(self);
}

static inline Event*
iterator_at(Iterator* self)
{
	return self->at(self);
}

static inline void
iterator_next(Iterator* self)
{
	self->next(self);
}

static inline void
iterator_close(Iterator* self)
{
	if (self->close)
		self->close(self);
}
