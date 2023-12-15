#pragma once

//
// noire
//
// time-series storage
//

typedef struct Comparator Comparator;

typedef int (*Compare)(void*, int, void*, int, void*);

struct Comparator
{
	Compare compare;
	void*   compare_arg;
};

hot static inline int
compare(Comparator* self, Row* a, Row* b)
{
	// compare by time first
	uint64_t diff = a->time - b->time;
	if (diff < 0)
		return -1;
	if (diff > 0)
		return 1;
	// compare data
	if (! self->compare)
		return 0;
	return self->compare(a->data, a->data_size,
	                     b->data, b->data_size,
	                     self->compare_arg);
}
