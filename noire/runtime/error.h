#pragma once

//
// noire
//
// time-series storage
//

typedef struct Error Error;

struct Error
{
	char        text[256];
	int         text_len;
	const char* file;
	const char* function;
	int         line;
	LogFunction log;
	void*       log_arg;
};

static inline void
error_init(Error* self)
{
	memset(self, 0, sizeof(*self));
}

static inline void no_return
error_throw(Error*        self,
            ExceptionMgr* mgr,
            const char*   file,
            const char*   function, int line,
            const char*   fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	self->file     = file;
	self->function = function;
	self->line     = line;
	self->text_len = vsnprintf(self->text, sizeof(self->text), fmt, args);
	if (self->log)
		self->log(self->log_arg, file, function, line, "error: ", fmt, args);
	va_end(args);
	exception_mgr_throw(mgr);
}
