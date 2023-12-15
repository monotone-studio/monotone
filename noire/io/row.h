#pragma once

//
// noire
//
// time-series storage
//

typedef struct Row Row;

struct Row
{
	uint8_t  flags;
	uint64_t time;
	uint32_t data_size;
	uint8_t  data[];
} packed;

always_inline hot static inline int
row_size(Row* self)
{
	return sizeof(Row) + self->data_size;
}

hot static inline Row*
row_create(uint64_t time, uint8_t* data, int data_size)
{
	auto self = (Row*)nr_malloc(sizeof(Row) + data_size);
	self->flags     = 0;
	self->time      = time;
	self->data_size = data_size;
	memcpy(self->data, data, data_size);
	return self;
}

static inline void
row_free(Row* self)
{
	nr_free(self);
}
