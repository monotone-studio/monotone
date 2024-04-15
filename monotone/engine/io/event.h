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

typedef struct EventArg EventArg;
typedef struct Event    Event;

enum
{
	MN_DELETE = 1
};

struct EventArg
{
	int      flags;
	uint64_t id;
	void*    key;
	size_t   key_size;
	void*    value;
	size_t   value_size;
};

struct Event
{
	uint64_t id;
	uint8_t  flags;
	uint8_t  key_size;
	uint32_t data_size;
	uint8_t  data[];
} packed;

always_inline hot static inline int
event_is_delete(Event* self)
{
	return self->flags & MN_DELETE;
}

always_inline hot static inline int
event_size(Event* self)
{
	return sizeof(Event) + self->key_size + self->data_size;
}

always_inline hot static inline uint8_t*
event_key(Event* self)
{
	return self->data;
}

always_inline hot static inline uint8_t*
event_value(Event* self)
{
	return self->data + self->key_size;
}

hot static inline void
event_init(Event* self, EventArg* arg)
{
	self->id        = arg->id;
	self->flags     = arg->flags;
	self->key_size  = arg->key_size;
	self->data_size = arg->key_size + arg->value_size;
	if (arg->key_size > 0)
		memcpy(self->data, arg->key, arg->key_size);
	if (arg->value_size > 0)
		memcpy(self->data + arg->key_size, arg->value, arg->value_size);
}

static inline Event*
event_allocate(Heap* heap, EventArg* arg)
{
	auto arg_size = arg->key_size + arg->value_size;
	auto event = (Event*)heap_allocate(heap, sizeof(Event) + arg_size);
	event_init(event, arg);
	return event;
}

static inline Event*
event_copy(Heap* heap, Event* src)
{
	auto event = (Event*)heap_allocate(heap, event_size(src));
	memcpy(event, src, event_size(src));
	return event;
}

static inline Event*
event_malloc(EventArg* arg)
{
	auto arg_size = arg->key_size + arg->value_size;
	auto event = (Event*)mn_malloc(sizeof(Event) + arg_size);
	event_init(event, arg);
	return event;
}

always_inline static inline void
event_free(Event* self)
{
	mn_free(self);
}
