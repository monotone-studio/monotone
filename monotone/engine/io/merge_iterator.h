#pragma once

//
// monotone
//
// time-series storage
//

typedef struct MergeIteratorRef MergeIteratorRef;
typedef struct MergeIterator    MergeIterator;

struct MergeIteratorRef
{
	Iterator* iterator;
	bool      advance;
};

struct MergeIterator
{
	Iterator    iterator;
	Iterator*   current;
	Buf         list;
	int         list_count;
	Comparator* comparator;
};

static inline void
merge_iterator_add(MergeIterator* self, Iterator* source)
{
	MergeIteratorRef ref =
	{
		.iterator = source,
		.advance  = false
	};
	buf_write(&self->list, &ref, sizeof(ref));
	self->list_count++;
}

static inline void
merge_iterator_open(MergeIterator* self, Comparator* comparator)
{
	self->comparator = comparator;
	iterator_next(&self->iterator);
}

static inline bool
merge_iterator_has(MergeIterator* self)
{
	return self->current != NULL;
}

static inline Row*
merge_iterator_at(MergeIterator* self)
{
	if (unlikely(self->current == NULL))
		return NULL;
	return iterator_at(self->current);
}

hot static inline void
merge_iterator_next(MergeIterator* self)
{
	auto list = (MergeIteratorRef*)self->list.start;
	for (int pos = 0; pos < self->list_count; pos++)
	{
		auto current = &list[pos];
		if (current->advance)
		{
			iterator_next(current->iterator);
			current->advance = false;
		}
	}
	self->current = NULL;

	Iterator* min_iterator = NULL;
	Row* min = NULL;
	for (int pos = 0; pos < self->list_count; pos++)
	{
		auto current = &list[pos];
		auto row = iterator_at(current->iterator);
		if (row == NULL)
			continue;
		if (min == NULL)
		{
			current->advance = true;
			min_iterator = current->iterator;
			min = row;
			continue;
		}
		auto rc = compare(self->comparator, min, row);
		if (rc == 0)
		{
			current->advance = true;
		} else
		if (rc > 0)
		{
			for (int i = 0; i < self->list_count; i++)
				list[i].advance = false;
			current->advance = true;
			min_iterator = current->iterator;
			min = row;
		}
	}
	self->current = min_iterator;
}

static inline void
merge_iterator_free(MergeIterator* self)
{
	buf_free(&self->list);
}

static inline void
merge_iterator_reset(MergeIterator* self)
{
	self->current    = NULL;
	self->list_count = 0;
	self->comparator = NULL;
	buf_reset(&self->list);
}

static inline void
merge_iterator_init(MergeIterator* self)
{
	self->current    = NULL;
	self->list_count = 0;
	self->comparator = NULL;
	buf_init(&self->list);
	auto it = &self->iterator;
	it->has   = (IteratorHas)merge_iterator_has;
	it->at    = (IteratorAt)merge_iterator_at;
	it->next  = (IteratorNext)merge_iterator_next;
	it->close = (IteratorClose)merge_iterator_free;
}
