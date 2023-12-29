#pragma once

//
// monotone
//
// time-series storage
//

typedef struct Config Config;

struct Config
{
	uint64_t interval;
	int      workers;
};

static inline void
config_init(Config* self)
{
	self->interval = 3000000;
	self->workers  = 1;
}

static inline void
config_free(Config* self)
{
	unused(self);
}
