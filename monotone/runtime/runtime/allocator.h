#pragma once

//
// monotone
//
// time-series storage
//

static inline void*
mn_malloc(size_t size)
{
	auto ptr = malloc(size);
	if (unlikely(ptr == NULL))
		error_system();
	return ptr;
}

static inline void*
mn_realloc(void* pointer, size_t size)
{
	auto ptr = realloc(pointer, size);
	if (unlikely(ptr == NULL))
		error_system();
	return ptr;
}

static inline void
mn_free(void* pointer)
{
	free(pointer);
}
