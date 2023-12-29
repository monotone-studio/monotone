
//
// monotone
//
// time-series storage
//

#include <monotone_runtime.h>

__thread Runtime mn_runtime;

void
runtime_init(void)
{
	exception_mgr_init(&mn_runtime.exception_mgr);
	error_init(&mn_runtime.error);
	mn_runtime.log     = NULL;
	mn_runtime.log_arg = NULL;
	mn_runtime.global  = NULL;
}

void
runtime_free(void)
{
}

void
runtime_set_global(void* ptr)
{
	mn_runtime.global = ptr;
}
