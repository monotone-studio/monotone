#pragma once

//
// monotone
//
// time-series storage
//

typedef struct Id Id;

enum
{
	ID_NONE             = 0,
	ID                  = 1 << 0,
	ID_INCOMPLETE       = 1 << 1,
	ID_COMPLETE         = 1 << 2,
	ID_CLOUD            = 1 << 3,
	ID_CLOUD_INCOMPLETE = 1 << 4
};

struct Id
{
	uint64_t min;
	uint64_t max;
} packed;

static inline void
id_path(Id* self, Source* source, int state, char* path)
{
	switch (state) {
	case ID:
		// <source_path>/<min>
		source_pathfmt(source, path, PATH_MAX, "%020" PRIu64, self->min);
		break;
	case ID_INCOMPLETE:
		// <source_path>/<min>.incomplete
		source_pathfmt(source, path, PATH_MAX, "%020" PRIu64 ".incomplete",
		               self->min);
		break;
	case ID_COMPLETE:
		// <source_path>/<min>.complete
		source_pathfmt(source, path, PATH_MAX, "%020" PRIu64 ".complete",
		               self->min);
		break;
	case ID_CLOUD:
		// <source_path>/<min>.cloud
		source_pathfmt(source, path, PATH_MAX, "%020" PRIu64 ".cloud",
		               self->min);
		break;
	case ID_CLOUD_INCOMPLETE:
		// <source_path>/<min>.cloud.incomplete
		source_pathfmt(source, path, PATH_MAX, "%020" PRIu64 ".cloud.incomplete",
		               self->min);
		break;
	default:
		abort();
		break;
	}
}
