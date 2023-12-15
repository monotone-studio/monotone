
//
// noire
//
// time-series storage
//

#include <noire_runtime.h>

__thread Runtime nr_runtime;

void
runtime_init(void* global)
{
	exception_mgr_init(&nr_runtime.exception_mgr);
	error_init(&nr_runtime.error);
	nr_runtime.log     = NULL;
	nr_runtime.log_arg = NULL;
	nr_runtime.global  = global;
}

void
runtime_free(void)
{
}
