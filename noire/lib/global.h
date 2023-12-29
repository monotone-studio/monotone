#pragma once

//
// noire
//
// time-series storage
//

typedef struct Global Global;

struct Global
{
	Config* config;
};

#define global() ((Global*)nr_runtime.global)
#define config()  global()->config
