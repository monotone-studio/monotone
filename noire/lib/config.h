#pragma once

//
// noire
//
// time-series storage
//

typedef struct Config Config;

struct Config
{
	Str      directory;
	uint64_t interval;
	uint32_t compression;
	uint32_t crc;
	uint32_t region_size;
	uint32_t compaction_wm;
	uint32_t workers;
};

static inline void
config_init(Config* self)
{
	self->interval      = 1048576;
	self->compression   = COMPRESSION_OFF;
	self->crc           = 0;
	self->region_size   = 128 * 1024;
	self->compaction_wm = 40 * 1024 * 1024;
	self->workers       = 1;
	str_init(&self->directory);
}

static inline void
config_free(Config* self)
{
	str_free(&self->directory);
}
