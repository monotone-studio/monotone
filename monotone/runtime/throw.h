#pragma once

//
// noire
//
// time-series storage
//

// try-catch
#define try(exception) \
	exception_mgr_try(&nr_runtime.exception_mgr, exception)

#define catch(exception) \
	exception_mgr_catch(&nr_runtime.exception_mgr, exception)

// throw
#define rethrow() \
	exception_mgr_throw(&nr_runtime.exception_mgr)

#define throw(fmt, ...) \
	error_throw(&nr_runtime.error, \
	            &nr_runtime.exception_mgr, \
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

// log
#define log(fmt, ...) \
	log_at(nr_runtime.log, \
	       nr_runtime.log_arg, \
	       source_file, \
	       source_function, \
	       source_line, "", fmt, ## __VA_ARGS__)
