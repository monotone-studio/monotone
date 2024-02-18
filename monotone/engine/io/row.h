#pragma once

//
// monotone
//
// time-series storage
//

typedef struct RowRef RowRef;
typedef struct Row    Row;

struct RowRef
{
	uint64_t time;
	void*    data;
	size_t   data_size;
	bool     remove;
};

struct Row
{
	uint64_t time;
	uint8_t  is_delete:1;
	uint8_t  flags:7;
	uint32_t data_size;
	uint8_t  data[];
} packed;

hot static inline void
row_init(Row* self, RowRef* ref)
{
	self->time      = ref->time;
	self->is_delete = ref->remove;
	self->flags     = 0;
	self->data_size = ref->data_size;
	if (ref->data)
		memcpy(self->data, ref->data, ref->data_size);
}

static inline Row*
row_allocate(Heap* heap, RowRef* ref)
{
	auto row = (Row*)heap_allocate(heap, sizeof(Row) + ref->data_size);
	row_init(row, ref);
	return row;
}

static inline Row*
row_malloc(RowRef* ref)
{
	auto row = (Row*)mn_malloc(sizeof(Row) + ref->data_size);
	row_init(row, ref);
	return row;
}

always_inline static inline void
row_free(Row* self)
{
	mn_free(self);
}

always_inline hot static inline int
row_size(Row* self)
{
	return sizeof(Row) + self->data_size;
}

always_inline hot static inline uint64_t
row_interval_min(Row* self)
{
	return self->time - (self->time % config_interval());
}
