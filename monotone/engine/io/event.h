#pragma once

//
// monotone
//
// time-series storage
//

typedef struct EventRef EventRef;
typedef struct Event    Event;

struct EventRef
{
	uint64_t time;
	void*    data;
	size_t   data_size;
	bool     remove;
};

struct Event
{
	uint64_t time;
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

always_inline hot static inline uint64_t
event_interval_min(Event* self)
{
	return self->time - (self->time % config_interval());
}

hot static inline void
event_init(Event* self, EventRef* ref)
{
	self->time      = ref->time;
	self->is_delete = ref->remove;
	self->flags     = 0;
	self->data_size = ref->data_size;
	if (ref->data)
		memcpy(self->data, ref->data, ref->data_size);
}

static inline Event*
event_allocate(Heap* heap, EventRef* ref)
{
	auto event = (Event*)heap_allocate(heap, sizeof(Event) + ref->data_size);
	event_init(event, ref);
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
event_malloc(EventRef* ref)
{
	auto event = (Event*)mn_malloc(sizeof(Event) + ref->data_size);
	event_init(event, ref);
	return event;
}

always_inline static inline void
event_free(Event* self)
{
	mn_free(self);
}
