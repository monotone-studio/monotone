#pragma once

//
// monotone
//
// time-series storage
//

typedef struct Region Region;

struct Region
{
	uint32_t size;
	uint32_t rows;
	uint32_t reserve[4];
	// u32 offset[]
	// data
} packed;

static inline Row*
region_get(Region* self, int pos)
{
	assert(pos < (int)self->rows);
	auto start  = (char*)self + sizeof(Region);
	auto offset = (uint32_t*)start;
	return (Row*)(start + (sizeof(uint32_t) * self->rows) +
	              offset[pos]);
}
