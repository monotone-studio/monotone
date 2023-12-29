#pragma once

//
// monotone
//
// time-series storage
//

typedef struct Row Row;

struct Row
{
	uint8_t  is_delete:1;
	uint8_t  flags:7;
	uint64_t time;
	uint32_t data_size;
	uint8_t  data[];
} packed;

hot static inline Row*
row_init(Row* self, uint64_t time, uint8_t* data, int data_size)
{
	self->is_delete = 0;
	self->flags     = 0;
	self->time      = time;
	self->data_size = data_size;
	memcpy(self->data, data, data_size);
	return self;
}

always_inline hot static inline int
row_size(Row* self)
{
	return sizeof(Row) + self->data_size;
}
