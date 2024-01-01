#pragma once

//
// monotone
//
// time-series storage
//

typedef struct Row Row;

struct Row
{
	uint64_t time;
	uint8_t  is_delete:1;
	uint8_t  flags:7;
	uint32_t data_size;
	uint8_t  data[];
} packed;

hot static inline Row*
row_init(Row* self, uint64_t time, uint8_t* data, int data_size)
{
	self->time      = time;
	self->is_delete = 0;
	self->flags     = 0;
	self->data_size = data_size;
	if (data)
		memcpy(self->data, data, data_size);
	return self;
}

static inline Row*
row_allocate(uint64_t time, uint8_t* data, int data_size)
{
	auto row = (Row*)mn_malloc(sizeof(Row) + data_size);
	row_init(row, time, data, data_size);
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
	return self->time - (self->time % config()->interval);
}
