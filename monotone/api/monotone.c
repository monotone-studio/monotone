
//
// monotone
//
// time-series storage
//

#include <monotone_runtime.h>
#include <monotone_lib.h>
#include <monotone_io.h>
#include <monotone_engine.h>
#include <monotone_instance.h>
#include <monotone_api.h>
#include <monotone.h>

MONOTONE_API monotone_t*
monotone_init(monotone_compare_t compare, void* compare_arg)
{
	monotone_t* self = malloc(sizeof(monotone_t));
	if (unlikely(self == NULL))
		return NULL;
	self->type = MONOTONE_OBJ;
	instance_init(&self->instance);
	self->instance.comparator.compare = (Compare)compare;
	self->instance.comparator.compare_arg = compare_arg;
	return self;
}

MONOTONE_API void
monotone_free(void* ptr)
{
	switch (*(int*)ptr) {
	case MONOTONE_OBJ:
	{
		monotone_t* self = ptr;
		runtime_init(&self->instance.global);
		Exception e;
		if (try(&e))
		{
			instance_stop(&self->instance);
			instance_free(&self->instance);
		}
		if (catch(&e))
		{ }
		break;
	}
	case MONOTONE_OBJ_CURSOR:
	{
		monotone_cursor_t* self = ptr;
		monotone_t* env = self->env;
		runtime_init(&env->instance.global);
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
	unused(self);
	return mn_runtime.error.text;
}

MONOTONE_API int
monotone_version(void)
{
	// todo
	return 0;
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
monotone_open(monotone_t* self, const char* options)
{
	int rc = 0;
	runtime_init(&self->instance.global);
	Exception e;
	if (try(&e))
	{
		instance_start(&self->instance, options);
	}
	if (catch(&e)) {
		rc = -1;
	}
	return rc;
}

MONOTONE_API monotone_stats_storage_t*
monotone_stats(monotone_t* self, monotone_stats_t* stats)
{
	StatsStorage* stats_storage = NULL;
	runtime_init(&self->instance.global);
	Exception e;
	if (try(&e)) {
		stats_storage = engine_stats(&self->instance.engine, (Stats*)stats);
	}
	if (catch(&e))
	{ }
	return (monotone_stats_storage_t*)stats_storage;
}

hot MONOTONE_API int
monotone_insert(monotone_t* self, monotone_row_t* row)
{
	runtime_init(&self->instance.global);
	int rc = 0;
	Exception e;
	if (try(&e)) {
		engine_write(&self->instance.engine, false, row->time,
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
	runtime_init(&self->instance.global);
	int rc = 0;
	Exception e;
	if (try(&e)) {
		engine_write(&self->instance.engine, true, row->time,
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
	runtime_init(&self->env->instance.global);
	int rc = 0;
	Exception e;
	if (try(&e)) {
		engine_write_by(&self->env->instance.engine, &self->cursor,
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
	runtime_init(&self->env->instance.global);
	int rc = 0;
	Exception e;
	if (try(&e)) {
		engine_write_by(&self->env->instance.engine, &self->cursor,
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
	runtime_init(&self->instance.global);

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
			engine_cursor_open(&cursor->cursor, &self->instance.engine, NULL);
		} else
		{
			auto key = row_allocate(row->time, row->data, row->data_size);
			guard(guard, row_free, key);
			engine_cursor_open(&cursor->cursor, &self->instance.engine, key);
		}
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
	runtime_init(&self->env->instance.global);
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
	runtime_init(&self->env->instance.global);
	int rc;
	Exception e;
	if (try(&e)) {
		engine_cursor_next(&self->cursor);
		rc = engine_cursor_has(&self->cursor);
	}
	if (catch(&e)) {
		rc = -1;
	}
	return rc;
}

MONOTONE_API int
monotone_checkpoint(monotone_t* self)
{
	int rc = 0;
	runtime_init(&self->instance.global);
	Exception e;
	if (try(&e))
	{
		engine_flush(&self->instance.engine);
	}
	if (catch(&e)) {
		rc = -1;
	}
	return rc;
}

MONOTONE_API int
monotone_drop(monotone_t* self, uint64_t min, uint64_t max)
{
	runtime_init(&self->instance.global);
	int rc = 0;
	Exception e;
	if (try(&e)) {
		(void)self;
		(void)min;
		(void)max;
		// todo
	}
	if (catch(&e)) {
		rc = -1;
	}
	return rc;
}
