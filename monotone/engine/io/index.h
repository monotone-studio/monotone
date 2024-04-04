#pragma once

//
// monotone
//
// time-series storage
//

typedef struct IndexRegion IndexRegion;
typedef struct Index       Index;

#define INDEX_MAGIC   0x20849615
#define INDEX_VERSION 0

struct IndexRegion
{
	uint32_t offset;
	uint32_t crc;
	uint64_t min;
	uint64_t max;
	uint32_t size;
	uint32_t size_origin;
	uint32_t events;
	uint32_t reserved[4];
} packed;

struct Index
{
	uint32_t crc;
	uint32_t crc_data;
	uint32_t magic;
	uint32_t version;
	Id       id;
	uint32_t size;
	uint32_t size_origin;
	uint64_t size_regions;
	uint64_t size_regions_origin;
	uint64_t size_total;
	uint64_t size_total_origin;
	uint32_t regions;
	uint64_t events;
	uint64_t time_create;
	uint64_t time_refresh;
	uint32_t refreshes;
	uint64_t lsn;
	uint8_t  compression;
	uint8_t  encryption;
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
	unused(self);
	assert(pos < (int)self->regions);
	return &((IndexRegion*)data->start)[pos];
}

static inline uint64_t
index_region_min(Index* self, Buf* data, int pos)
{
	return index_get(self, data, pos)->min;
}

static inline uint64_t
index_region_max(Index* self, Buf* data, int pos)
{
	return index_get(self, data, pos)->max;
}

static inline uint64_t
index_min(Index* self, Buf* data)
{
	return index_region_min(self, data, 0);
}

static inline uint64_t
index_max(Index* self, Buf* data)
{
	return index_region_max(self, data, self->regions - 1);
}
