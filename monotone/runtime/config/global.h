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
	Control* control;
	UuidMgr* uuid_mgr;
};

#define global() ((Global*)mn_runtime.context->global)
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

static inline int
config_workers(void)
{
	return var_int_of(&config()->workers);
}

// control
static inline void
control_save_config(void)
{
	global()->control->save_config(global()->control->arg);
}

static inline void
control_lock_shared(void)
{
	global()->control->lock(global()->control->arg, true);
}

static inline void
control_lock_exclusive(void)
{
	global()->control->lock(global()->control->arg, false);
}

static inline void
control_unlock(void)
{
	global()->control->unlock(global()->control->arg);
}

static inline void
control_unlock_guard(void* _unused)
{
	unused(_unused);
	control_unlock();
}
