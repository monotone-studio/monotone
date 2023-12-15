#pragma once

//
// noire
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
};

extern __thread Runtime nr_runtime;

void runtime_init(void);
void runtime_free(void);
