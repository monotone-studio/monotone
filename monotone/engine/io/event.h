#pragma once

//
// monotone
//
// time-series storage
//

typedef struct EventArg EventArg;
typedef struct Event    Event;

struct EventArg
{
	uint64_t id;
	void*    data;
	size_t   data_size;
	bool     remove;
};

struct Event
{
	uint64_t id;
	uint8_t  is_delete:1;
	uint8_t  flags:7;
	uint32_t data_size;
	uint8_t  data[];
} packed;

always_inline hot static inline int
event_size(Event* self)
{
	return sizeof(Event) + self->data_size;
}

hot static inline void
event_init(Event* self, EventArg* arg)
{
	self->id        = arg->id;
	self->is_delete = arg->remove;
	self->flags     = 0;
	self->data_size = arg->data_size;
	if (arg->data)
		memcpy(self->data, arg->data, arg->data_size);
}

static inline Event*
event_allocate(Heap* heap, EventArg* arg)
{
	auto event = (Event*)heap_allocate(heap, sizeof(Event) + arg->data_size);
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
	auto event = (Event*)mn_malloc(sizeof(Event) + arg->data_size);
	event_init(event, arg);
	return event;
}

always_inline static inline void
event_free(Event* self)
{
	mn_free(self);
}
