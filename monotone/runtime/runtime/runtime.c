
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
	mn_runtime.context = context;
}
