
//
// monotone
//
// time-series storage
//

#include <monotone_runtime.h>

__thread Runtime mn_runtime;

void
runtime_init(Context* context)
{
	exception_mgr_init(&mn_runtime.exception_mgr);
	error_init(&mn_runtime.error, context->log, context->log_arg);
	mn_runtime.context = context;
}
