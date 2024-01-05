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

typedef struct monotone               monotone_t;
typedef struct monotone_stats         monotone_stats_t;
typedef struct monotone_stats_storage monotone_stats_storage_t;
typedef struct monotone_cursor        monotone_cursor_t;
typedef struct monotone_row           monotone_row_t;

typedef int64_t (*monotone_compare_t)(uint64_t l_time, void* l,
                                      uint64_t r_time, void* r,
                                      void*    arg);

struct monotone_stats
{
	uint64_t lsn;
	uint64_t rows_written;
	uint64_t rows_written_bytes;
	uint32_t storages;
	uint64_t reserved[8];
};

struct monotone_stats_storage
{
	const char* name;
	uint64_t    partitions;
	uint64_t    pending;
	uint64_t    min;
	uint64_t    max;
	uint64_t    rows;
	uint64_t    rows_cached;
	uint64_t    size;
	uint64_t    size_uncompressed;
	uint64_t    size_cached;
	uint64_t    reserved[8];
};

struct monotone_row
{
	uint64_t time;
	void*    data;
	size_t   data_size;
};

// main
MONOTONE_API monotone_t*
monotone_init(monotone_compare_t, void* compare_arg);

MONOTONE_API void
monotone_free(void*);

MONOTONE_API const char*
monotone_error(monotone_t*);

MONOTONE_API int
monotone_version(void);

MONOTONE_API uint64_t
monotone_now(monotone_t*);

// storage
MONOTONE_API int
monotone_open(monotone_t*, const char* options);

MONOTONE_API monotone_stats_storage_t*
monotone_stats(monotone_t*, monotone_stats_t*);

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

// data management
MONOTONE_API int
monotone_checkpoint(monotone_t*, uint64_t before);

MONOTONE_API int
monotone_drop(monotone_t*, uint64_t min, uint64_t max);

#ifdef __cplusplus
}
#endif

#endif // MONOTONE_H
