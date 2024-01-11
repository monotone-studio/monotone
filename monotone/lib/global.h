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

#define global() ((Global*)mn_runtime.global)
#define config()  global()->config

static inline const char*
config_directory(void)
{
	return var_string_of(&config()->directory);
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

static inline void
config_update(void)
{
	char path[PATH_MAX];
	snprintf(path, sizeof(path), "%s/config.json",
	         config_directory());
	config_save(config(), path);
}
