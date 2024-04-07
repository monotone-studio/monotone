#pragma once

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

static inline void
guard_runner(Guard* self)
{
	if (self->function)
	{
		self->function(self->function_arg);
		self->function = NULL;
	}

	// always last unless explicit call
	auto exception = mn_runtime.exception_mgr.last;
	assert(exception->guard_stack == self);
	exception->guard_stack = self->prev;
}

static inline void*
unguard(Guard* self)
{
	self->function = NULL;
	return self->function_arg;
}

#define guard_as(self, func, func_arg) \
	Guard __attribute((cleanup(guard_runner))) self = { \
		.function = (GuardFunction)func, \
		.function_arg = func_arg, \
		.prev = ({ \
			auto exception = mn_runtime.exception_mgr.last; \
			auto prev = exception->guard_stack; \
			exception->guard_stack = &self; \
			prev; \
		}) \
	}

#define guard_auto_def(name, line, func, func_arg) \
	guard_as(name##line, func, func_arg)

#define guard_auto(name, line, func, func_arg) \
	guard_auto_def(name, line, func, func_arg)

#define guard(func, func_arg) \
	guard_auto(_guard_, __LINE__, func, func_arg)
