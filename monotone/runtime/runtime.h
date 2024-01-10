#pragma once

//
// monotone
//
// time-series storage
//

typedef struct Runtime Runtime;

struct Runtime
{
	ExceptionMgr exception_mgr;
	Error        error;
	LogFunction  log;
	void*        log_arg;
	void*        global;
};

extern __thread Runtime mn_runtime;

void runtime_init(LogFunction, void*, void*);
