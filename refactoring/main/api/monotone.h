#ifndef MONOTONE_H
#define MONOTONE_H

//
// monotone
//
// time-series storage
//

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#define MONOTONE_API __attribute__((visibility("default")))

typedef struct monotone        monotone_t;
typedef struct monotone_cursor monotone_cursor_t;
typedef struct monotone_row    monotone_row_t;

typedef int64_t (*monotone_compare_t)(uint64_t l_time, void* l,
                                      uint64_t r_time, void* r,
                                      void*    arg);

struct monotone_row
{
	uint64_t time;
	void*    data;
	size_t   data_size;
};

// environment
MONOTONE_API monotone_t*
monotone_init(monotone_compare_t, void* compare_arg);

MONOTONE_API void
monotone_free(void*);

MONOTONE_API const char*
monotone_error(monotone_t*);

MONOTONE_API uint64_t
monotone_now(monotone_t*);

// main
MONOTONE_API int
monotone_execute(monotone_t*, const char* command, char** result);

MONOTONE_API int
monotone_open(monotone_t*, const char* path);

// insert, update, delete
MONOTONE_API int
monotone_insert(monotone_t*, monotone_row_t*);

MONOTONE_API int
monotone_delete(monotone_t*, monotone_row_t*);

MONOTONE_API int
monotone_delete_by(monotone_cursor_t*);

MONOTONE_API int
monotone_update_by(monotone_cursor_t*, monotone_row_t*);

// cursor
MONOTONE_API monotone_cursor_t*
monotone_cursor(monotone_t*, monotone_row_t*);

MONOTONE_API int
monotone_read(monotone_cursor_t*, monotone_row_t*);

MONOTONE_API int
monotone_next(monotone_cursor_t*);

#ifdef __cplusplus
}
#endif

#endif // MONOTONE_H