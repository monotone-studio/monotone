#pragma once

//
// monotone
//
// time-series storage
//

static inline int
test_sh(TestSuite* self, const char* fmt, ...)
{
	(void)self;
	va_list args;
	va_start(args, fmt);
	char cmd[PATH_MAX];
	vsnprintf(cmd, sizeof(cmd), fmt, args);
	int rc = system(cmd);
	va_end(args);
	return rc;
}

static inline void
test_info(TestSuite* self, const char* fmt, ...)
{
	(void)self;
	va_list args;
	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
	fflush(stdout);
}

static inline void
test_error(TestSuite* self, const char* fmt, ...)
{
	(void)self;
	va_list args;
	va_start(args, fmt);
	printf("\n");
	printf("error: ");
	vprintf(fmt, args);
	printf("\n");
	va_end(args);
	fflush(stdout);
}

static inline void
test_log(TestSuite* self, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vfprintf(self->current_test_result, fmt, args);
	va_end(args);
	fflush(self->current_test_result);
}

static inline void
test_log_error(TestSuite* self)
{
	if (! self->env)
	{
		test_log(self, "error: env is not openned\n");
		return;
	}
	auto error = monotone_error(self->env);
	if (error)
		test_log(self, "error: %s\n", error);
	else
		test_log(self, "(null)\n");
}

#define test(expr) ({ \
	if (! (expr)) { \
		fprintf(stdout, "fail (%s:%d) %s\n", __FILE__, __LINE__, #expr); \
		fflush(stdout); \
		abort(); \
	} \
})

static inline char*
test_chomp(char* start)
{
	if (start == NULL)
		return NULL;
	char* pos = start;
	while (*pos != '\n')
		pos++;
	*pos = 0;
	return start;
}

static inline char*
test_arg(char** start)
{
	if (*start == NULL)
		return NULL;
	char* pos = *start;
	while (*pos == ' ')
		pos++;
	char* arg = pos;
	while (*pos != '\n' && *pos != ' ')
		pos++;
	int is_eol = *pos == '\n';
	*pos = 0;
	pos++;
	*start = (is_eol) ? NULL : pos;
	if (*arg == 0)
		return NULL;
	return arg;
}
