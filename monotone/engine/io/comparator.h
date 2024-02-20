#pragma once

//
// monotone
//
// time-series storage
//

typedef struct Comparator Comparator;

typedef int (*Compare)(EventArg*, EventArg*, void*);

struct Comparator
{
	Compare compare;
	void*   compare_arg;
};

static inline void
comparator_init(Comparator* self)
{
	self->compare     = NULL;
	self->compare_arg = NULL;
}

hot static inline int64_t
compare(Comparator* self, Event* a, Event* b)
{
	// compare by time first
	int64_t diff = a->time - b->time;
	if (likely(diff != 0))
		return diff;
	if (! self->compare)
		return 0;
	EventArg l =
	{
		.time      = a->time,
		.data_size = a->data_size,
		.data      = a->data,
		.remove    = a->is_delete
	};
	EventArg r =
	{
		.time      = b->time,
		.data_size = b->data_size,
		.data      = b->data,
		.remove    = b->is_delete
	};
	return self->compare(&l, &r, self->compare_arg);
}
