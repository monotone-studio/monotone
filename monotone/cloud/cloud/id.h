#pragma once

//
// monotone
//
// time-series storage
//

typedef struct Id Id;

struct Id
{
	uint64_t min;
	uint64_t max;
} packed;

static inline void
id_path(Id* self, Source* source, char* path)
{
	// <source_path>/<min>
	source_pathfmt(source, path, PATH_MAX, "%020" PRIu64, self->min);
}

static inline void
id_path_incomplete(Id* self, Source* source, char* path)
{
	// <source_path>/<min>.incomplete
	source_pathfmt(source, path, PATH_MAX, "%020" PRIu64 ".incomplete",
	               self->min);
}

static inline void
id_path_complete(Id* self, Source* source, char* path)
{
	// <source_path>/<min>.complete
	source_pathfmt(source, path, PATH_MAX, "%020" PRIu64 ".complete",
	               self->min);
}

static inline void
id_path_cloud(Id* self, Source* source, char* path)
{
	// <source_path>/<min>.cloud
	source_pathfmt(source, path, PATH_MAX, "%020" PRIu64 ".cloud",
	               self->min);
}

static inline void
id_path_cloud_incomplete(Id* self, Source* source, char* path)
{
	// <source_path>/<min>.cloud.incomplete
	source_pathfmt(source, path, PATH_MAX, "%020" PRIu64 ".cloud.incomplete",
	               self->min);
}
