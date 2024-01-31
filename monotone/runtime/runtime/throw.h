#pragma once

//
// monotone
//
// time-series storage
//

// try-catch
#define try(exception) \
	exception_mgr_try(&mn_runtime.exception_mgr, exception)

#define catch(exception) \
	exception_mgr_catch(&mn_runtime.exception_mgr, exception)

// throw
#define rethrow() \
	exception_mgr_throw(&mn_runtime.exception_mgr)

#define throw(fmt, ...) \
	error_throw(mn_runtime.context->error, \
	            &mn_runtime.exception_mgr, \
	            source_file, \
	            source_function, \
	            source_line, \
	            fmt, ## __VA_ARGS__)

// errors
#define error(fmt, ...) \
	throw(fmt, ## __VA_ARGS__)

#define error_system() \
	throw("%s(): %s (errno: %d)", source_function, \
	      strerror(errno), errno)

#define error_data() \
	throw("data read error")

// log
#define log(fmt, ...) \
	log_at(mn_runtime.context->log, \
	       mn_runtime.context->log_arg, \
	       source_file, \
	       source_function, \
	       source_line, fmt, ## __VA_ARGS__)
