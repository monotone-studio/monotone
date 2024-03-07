#pragma once

//
// monotone
//
// time-series storage
//

typedef struct IndexRegion IndexRegion;
typedef struct Index       Index;

#define INDEX_MAGIC 0x20849615

struct IndexRegion
{
	uint32_t offset;
	uint32_t size;
	uint32_t size_origin;
	uint32_t size_key_min;
	uint32_t size_key_max;
	uint32_t events;
	uint32_t crc;
	uint32_t reserved[4];
} packed;

struct Index
{
	uint32_t crc;
	uint32_t crc_data;
	uint32_t magic;
	Id       id;
	uint32_t size;
	uint32_t size_origin;
	uint64_t size_regions;
	uint64_t size_regions_origin;
	uint64_t size_total;
	uint64_t size_total_origin;
	uint32_t regions;
	uint64_t events;
	uint64_t time;
	uint64_t lsn;
	uint8_t  compression;
	uint32_t reserved[4];
} packed;

static inline void
index_init(Index* self)
{
	memset(self, 0, sizeof(*self));
}

static inline IndexRegion*
index_get(Index* self, Buf* data, int pos)
{
	assert(pos < (int)self->regions);
	auto offset = (uint32_t*)data->start;
	return (IndexRegion*)(data->start + (sizeof(uint32_t) * self->regions) +
	                      offset[pos]);
}

static inline Event*
index_region_min(Index* self, Buf* data, int pos)
{
	auto region = index_get(self, data, pos);
	return (Event*)((char*)region + sizeof(IndexRegion));
}

static inline Event*
index_region_max(Index* self, Buf* data, int pos)
{
	auto region = index_get(self, data, pos);
	return (Event*)((char*)region + sizeof(IndexRegion) +
	                region->size_key_min);
}

static inline Event*
index_min(Index* self, Buf* data)
{
	return index_region_min(self, data, 0);
}

static inline Event*
index_max(Index* self, Buf* data)
{
	return index_region_max(self, data, self->regions - 1);
}
