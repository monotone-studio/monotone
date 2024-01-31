
//
// monotone
//
// time-series storage
//

#include <monotone_private.h>
#include <monotone.h>

enum
{
	MONOTONE_OBJ        = 0x3fb15941,
	MONOTONE_OBJ_CURSOR = 0x143BAF02,
	MONOTONE_OBJ_FREED  = 0x28101019
};

struct monotone
{
	int  type;
	Main main;
};

struct monotone_cursor
{
	int              type;
	EngineCursor     cursor;
	struct monotone* env;
};

static inline void
monotone_enter(monotone_t* self)
{
	error_reset(&self->main.error);
	runtime_init(&self->main.context);
}

MONOTONE_API monotone_t*
monotone_init(monotone_compare_t compare, void* compare_arg)
{
	monotone_t* self = malloc(sizeof(monotone_t));
	if (unlikely(self == NULL))
		return NULL;
	self->type = MONOTONE_OBJ;
	main_init(&self->main);

	monotone_enter(self);
	Exception e;
	if (try(&e))
	{
		(void)compare;
		(void)compare_arg;
		main_prepare(&self->main);
	}
	if (catch(&e)) {
		main_free(&self->main);
		free(self);
		self = NULL;
	}
	return self;
}

MONOTONE_API void
monotone_free(void* ptr)
{
	switch (*(int*)ptr) {
	case MONOTONE_OBJ:
	{
		monotone_t* self = ptr;
		monotone_enter(self);
		Exception e;
		if (try(&e))
		{
			main_stop(&self->main);
			main_free(&self->main);
		}
		if (catch(&e))
		{ }
		break;
	}
	case MONOTONE_OBJ_CURSOR:
	{
		monotone_cursor_t* self = ptr;
		monotone_t* env = self->env;
		monotone_enter(env);
		Exception e;
		if (try(&e)) {
			engine_cursor_close(&self->cursor);
		}
		if (catch(&e))
		{ }
		break;
	}
	case MONOTONE_OBJ_FREED:
		fprintf(stderr, "\n%s(%p): attempt to use freed object\n",
		        source_function, ptr);
		fflush(stderr);
		abort();
		break;
	default:
		fprintf(stderr, "\n%s(%p): unexpected object type\n",
		        source_function, ptr);
		fflush(stderr);
		abort();
		return;
	}

	*(int*)ptr = MONOTONE_OBJ_FREED;
	free(ptr);
}

MONOTONE_API const char*
monotone_error(monotone_t* self)
{
	return self->main.error.text;
}

hot MONOTONE_API uint64_t
monotone_now(monotone_t* self)
{
	unused(self);
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	uint64_t time_ns = t.tv_sec * (uint64_t)1e9 + t.tv_nsec;
	// us
	return time_ns / 1000;
}

MONOTONE_API int
monotone_open(monotone_t* self, const char* directory)
{
	int rc = 0;
	monotone_enter(self);
	Exception e;
	if (try(&e))
	{
		main_start(&self->main, directory);
	}
	if (catch(&e)) {
		rc = -1;
	}
	return rc;
}

MONOTONE_API int
monotone_execute(monotone_t* self, const char* command, char** result)
{
	int rc = 0;
	monotone_enter(self);
	Exception e;
	if (try(&e))
	{
		main_execute(&self->main, command, result);
	}
	if (catch(&e)) {
		rc = -1;
	}
	return rc;
}

hot MONOTONE_API int
monotone_insert(monotone_t* self, monotone_row_t* row)
{
	monotone_enter(self);
	int rc = 0;
	Exception e;
	if (try(&e)) {
		engine_write(&self->main.engine, false, row->time,
		             row->data,
		             row->data_size);
	}
	if (catch(&e)) {
		rc = -1;
	}
	return rc;
}

hot MONOTONE_API int
monotone_delete(monotone_t* self, monotone_row_t* row)
{
	monotone_enter(self);
	int rc = 0;
	Exception e;
	if (try(&e)) {
		engine_write(&self->main.engine, true, row->time,
		             row->data,
		             row->data_size);
	}
	if (catch(&e)) {
		rc = -1;
	}
	return rc;
}

hot MONOTONE_API int
monotone_delete_by(monotone_cursor_t* self)
{
	monotone_enter(self->env);
	int rc = 0;
	Exception e;
	if (try(&e)) {
		engine_write_by(&self->env->main.engine, &self->cursor,
		                true, 0, NULL, 0);
	}
	if (catch(&e)) {
		rc = -1;
	}
	return rc;
}

hot MONOTONE_API int
monotone_update_by(monotone_cursor_t* self, monotone_row_t* row)
{
	monotone_enter(self->env);
	int rc = 0;
	Exception e;
	if (try(&e)) {
		engine_write_by(&self->env->main.engine, &self->cursor,
		                false, row->time,
		                row->data, row->data_size);
	}
	if (catch(&e)) {
		rc = -1;
	}
	return rc;
}

hot MONOTONE_API monotone_cursor_t*
monotone_cursor(monotone_t* self, monotone_row_t* row)
{
	monotone_enter(self);

	monotone_cursor_t* cursor;
	cursor = malloc(sizeof(monotone_cursor_t));
	if (unlikely(cursor == NULL))
		return NULL;
	cursor->type = MONOTONE_OBJ_CURSOR;
	cursor->env  = self;
	engine_cursor_init(&cursor->cursor);

	Exception e;
	if (try(&e))
	{
		if (row == NULL) {
			engine_cursor_open(&cursor->cursor, &self->main.engine, NULL);
		} else
		{
			auto key = row_allocate(row->time, row->data, row->data_size);
			guard(guard, row_free, key);
			engine_cursor_open(&cursor->cursor, &self->main.engine, key);
		}
		engine_cursor_skip_deletes(&cursor->cursor);
	}
	if (catch(&e)) {
		monotone_free(cursor);
		cursor = NULL;
	}

	return cursor;
}

hot MONOTONE_API int
monotone_read(monotone_cursor_t* self, monotone_row_t* row)
{
	monotone_enter(self->env);
	int rc;
	Exception e;
	if (try(&e))
	{
		auto at = engine_cursor_at(&self->cursor);
		if (likely(at)) {
			row->time      = at->time;
			row->data_size = at->data_size;
			row->data      = at->data;
			rc = 1;
		} else {
			rc = 0;
		}
	}
	if (catch(&e)) {
		rc = -1;
	}
	return rc;
}

hot MONOTONE_API int
monotone_next(monotone_cursor_t* self)
{
	monotone_enter(self->env);
	int rc;
	Exception e;
	if (try(&e)) {
		engine_cursor_next(&self->cursor);
		engine_cursor_skip_deletes(&self->cursor);
		rc = engine_cursor_has(&self->cursor);
	}
	if (catch(&e)) {
		rc = -1;
	}
	return rc;
}
