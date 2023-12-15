#pragma once

//
// noire
//
// time-series storage
//

// compiler
#include "runtime/c.h"
#include "runtime/macro.h"

// os
#include "runtime/atomic.h"
#include "runtime/spinlock.h"
#include "runtime/mutex.h"
#include "runtime/cond_var.h"
#include "runtime/thread.h"

// basic data structures
#include "runtime/list.h"

// exceptions
#include "runtime/exception.h"
#include "runtime/log.h"
#include "runtime/error.h"

// runtime
#include "runtime/runtime.h"
#include "runtime/throw.h"
#include "runtime/guard.h"

// memory
#include "runtime/allocator.h"
#include "runtime/str.h"
#include "runtime/buf.h"

#if 0
// file io
#include "io/iov.h"
#include "io/vfs.h"
#include "io/fs.h"
#include "io/file.h"
#include "io/logger.h"

// blob
#include "io/blob.h"

// tls
#include "io/tls_lib.h"
#include "io/tls_context.h"
#include "io/tls.h"

// network io
#include "io/socket.h"
#include "io/poll.h"
#include "io/tcp.h"
#include "io/listen.h"

// resolver
#include "io/resolver.h"
#endif
