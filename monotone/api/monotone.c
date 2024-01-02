
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
#include <monotone.h>

enum {
	MONOTONE_OBJ        = 0x3fb15941,
	MONOTONE_OBJ_CURSOR = 0x143BAF02,
	MONOTONE_OBJ_FREED  = 0x28101019
};

struct monotone
{
	int      type;
	Instance instance;
};

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
		// todo
		break;
	}
	case MONOTONE_OBJ_FREED:
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
		instance_start(&self->instance, options);
	if (catch(&e))
		rc = -1;
	return rc;
}
