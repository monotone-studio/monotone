
//
// monotone
//
// time-series storage
//

#include <monotone_runtime.h>

__thread Runtime mn_runtime;

void
runtime_init(LogFunction log, void* log_arg, void* global)
{
	exception_mgr_init(&mn_runtime.exception_mgr);
	error_init(&mn_runtime.error);
	mn_runtime.log     = log;
	mn_runtime.log_arg = log_arg;
	mn_runtime.global  = global;
}
