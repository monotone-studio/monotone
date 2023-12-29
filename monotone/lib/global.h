#pragma once

//
// monotone
//
// time-series storage
//

typedef struct Global Global;

struct Global
{
	Config* config;
};

#define global() ((Global*)mn_runtime.global)
#define config()  global()->config
