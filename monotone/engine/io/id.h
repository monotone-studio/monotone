#pragma once

//
// monotone
//
// time-series storage
//

typedef struct Id Id;

struct Id
{
	uint64_t id;
	uint64_t id_parent;
	uint64_t min;
	uint64_t max;
};

static inline void
id_path(Id* self, Source* source, char* path)
{
	// <source_path>/<min>.<id>
	source_pathfmt(source, path, PATH_MAX, "%020" PRIu64 ".%020" PRIu64,
	               self->min, self->id);
}

static inline void
id_path_incomplete(Id* self, Source* source, char* path)
{
	// <source_path>/<min>.<id>.<id_parent>
	source_pathfmt(source, path, PATH_MAX, "%020" PRIu64 ".%020" PRIu64 ".%020" PRIu64,
	               self->min, self->id, self->id_parent);
}
