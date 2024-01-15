#pragma once

//
// monotone
//
// time-series storage
//

typedef struct Context Context;
typedef struct Runtime Runtime;

struct Context
{
	LogFunction log;
	void*       log_arg;
	void*       global;
};

struct Runtime
{
	ExceptionMgr exception_mgr;
	Error        error;
	Context*     context;
};

extern __thread Runtime mn_runtime;

void runtime_init(Context*);
