#pragma once

//
// monotone
//
// time-series storage
//

// enter/leave
#define enter(exception) \
	exception_mgr_enter(&mn_runtime.exception_mgr, exception)

#define leave(exception) \
	exception_mgr_leave(&mn_runtime.exception_mgr, exception)

// throw
#define rethrow() \
	exception_mgr_throw(&mn_runtime.exception_mgr)

#define error_throw(fmt, ...) \
	error_throw_as(&mn_runtime.error, \
	               &mn_runtime.exception_mgr, \
	               source_file, \
	               source_function, \
	               source_line, \
	               fmt, ## __VA_ARGS__)

// errors
#define error(fmt, ...) \
	error_throw(fmt, ## __VA_ARGS__)

#define error_system() \
	error_throw("%s(): %s (errno: %d)", source_function, \
	            strerror(errno), errno)

#define error_data() \
	error_throw("data read error")

// log
#define log(fmt, ...) \
	log_at(mn_runtime.context->log, \
	       mn_runtime.context->log_arg, \
	       source_file, \
	       source_function, \
	       source_line, fmt, ## __VA_ARGS__)
