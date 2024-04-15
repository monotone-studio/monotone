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

typedef struct Comparator Comparator;

struct Comparator
{
	int unused;
};

static inline void
comparator_init(Comparator* self)
{
	self->unused = 0;
}

hot static inline int64_t
compare(Comparator* self, Event* a, Event* b)
{
	unused(self);
	// compare by [id, key]
	int64_t diff = a->id - b->id;
	if (likely(diff != 0))
		return diff;
	if (a->key_size == 0 && b->key_size == 0)
		return 0;
	int size;
	if (a->key_size < b->key_size)
		size = a->key_size;
	else
		size = b->key_size;
	int rc = memcmp(a->data, b->data, size);
	if (rc == 0)
		return (int64_t)a->key_size - (int64_t)b->key_size;
	return rc;
}
