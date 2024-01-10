#pragma once

//
// monotone
//
// time-series storage
//

typedef struct Config Config;

struct Config
{
	Str      path;
	uint64_t interval;
	int      workers;
};

static inline void
config_init(Config* self)
{
	self->interval = 3000000;
	self->workers  = 1;
	str_init(&self->path);
}

static inline void
config_free(Config* self)
{
	str_free(&self->path);
}
