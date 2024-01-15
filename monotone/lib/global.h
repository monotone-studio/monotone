#pragma once

//
// monotone
//
// time-series storage
//

typedef struct Global Global;

struct Global
{
	Config*  config;
	UuidMgr* uuid_mgr;
};

#define global() ((Global*)mn_runtime.context->global)
#define config()  global()->config

static inline const char*
config_directory(void)
{
	return var_string_of(&config()->directory);
}

static inline void
config_update(void)
{
	char path[PATH_MAX];
	snprintf(path, sizeof(path), "%s/config.json",
	         config_directory());
	config_save(config(), path);
}

static inline bool
config_online(void)
{
	return var_int_of(&config()->online);
}

static inline uint64_t
config_interval(void)
{
	return var_int_of(&config()->interval);
}

static inline int
config_workers(void)
{
	return var_int_of(&config()->workers);
}

// psn
static inline uint64_t
config_psn(void)
{
	return var_int_of(&config()->psn);
}

static inline void
config_psn_set(uint64_t value)
{
	var_int_set(&config()->psn, value);
}

static inline uint64_t
config_psn_next(void)
{
	return var_int_set_next(&config()->psn);
}

static inline void
config_psn_follow(uint64_t value)
{
	var_int_follow(&config()->psn, value);
}
