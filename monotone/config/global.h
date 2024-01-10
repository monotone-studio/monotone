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
config_path(void)
{
	return str_of(&config()->path);
}
