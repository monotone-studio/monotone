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

extern __thread Runtime nr_runtime;

void runtime_init(void);
void runtime_free(void);
void runtime_set_global(void*);
