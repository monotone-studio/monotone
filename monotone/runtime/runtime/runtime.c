
//
// monotone.
//
// embeddable cloud-native storage for events
// and time-series data.
//
// Copyright (c) 2023-2024 Dmitry Simonenko
// Copyright (c) 2023-2024 Monotone
//
// Distributed under the terms of GNU LGPL v3 or
// Monotone Commercial License.
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
