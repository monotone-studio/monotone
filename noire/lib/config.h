#pragma once

//
// noire
//
// time-series storage
//

typedef struct Config Config;

struct Config
{
	// main
	Str directory;
};

static inline void
config_init(Config* self)
{
	str_init(&self->directory);
}

static inline void
config_free(Config* self)
{
	str_free(&self->directory);
}
