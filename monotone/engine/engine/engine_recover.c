
//
// monotone
//
// time-series storage
//

#include <monotone_runtime.h>
#include <monotone_lib.h>
#include <monotone_config.h>
#include <monotone_io.h>
#include <monotone_storage.h>
#include <monotone_engine.h>

static inline int
engine_recover_id_read(char** pos, uint64_t* id)
{
	char* path = *pos;
	*id = 0;
	while (*path && *path != '.')
	{
		if (unlikely(! isdigit(*path)))
			return -1;
		*id = (*id * 10) + *path - '0';
		path++;
	}
	*pos = path;
	return 0;
}

static inline int
engine_recover_id(char*     path,
                  uint64_t* min,
                  uint64_t* id,
                  uint64_t* id_parent)
{
	// <min>.<id>
	// <min>.<id>.<id_parent>

	// min
	if (engine_recover_id_read(&path, min) == -1)
		return -1;
	if (*path != '.')
		return -1;
	path++;

	// id
	if (engine_recover_id_read(&path, id) == -1)
		return -1;
	if (! *path)
	{
		*id_parent = *id;
		return 0;
	}
	if (*path != '.')
		return -1;
	path++;

	// id_parent
	return engine_recover_id_read(&path, id_parent);
}

static void
engine_recover_storage(Engine* self, Storage* storage)
{
	auto source = storage->source;

	// read storage directory
	char path[PATH_MAX];
	source_pathfmt(source, path, sizeof(path), "");

	DIR* dir = opendir(path);
	if (unlikely(dir == NULL))
		error("storage: directory '%s' open error", path);
	guard(dir_guard, closedir, dir);
	for (;;)
	{
		auto entry = readdir(dir);
		if (entry == NULL)
			break;
		if (entry->d_name[0] == '.')
			continue;

		// <min>.<id>
		// <min>.<id>.<id_parent>
		uint64_t min        = 0;
		uint64_t psn        = 0;
		uint64_t psn_parent = 0;
		if (engine_recover_id(entry->d_name, &min, &psn, &psn_parent) == -1)
			continue;

		PartId id =
		{
			.id        = psn,
			.id_parent = psn_parent,
			.min       = min,
			.max       = min + config_interval()
		};
		auto part = part_allocate(self->comparator, storage->source, &id);
		storage_add(storage, part);
	}
}

static void
engine_recover_validate(Engine* self, Storage* storage)
{
	auto storage_mgr = &self->storage_mgr;
	list_foreach_safe(&storage->list)
	{
		auto part = list_at(Part, link);

		// sync psn
		config_psn_follow(part->id.id);
		config_psn_follow(part->id.id_parent);

		// <min>.<id>.<id_parent>
		if (part->id.id != part->id.id_parent)
		{
			auto parent = storage_mgr_find_part(storage_mgr, part->id.id_parent);
			if (parent)
			{
				// parent still exists, remove incomplete partition
				storage_remove(storage, part);
				part_delete(part, true);
				part_free(part);
				continue;
			}

			// parent has been removed after sync during compaction

			// rename to completion
			part_rename(part);
			part->id.id_parent = part->id.id;
		}
	}
}

void
engine_recover(Engine* self)
{
	auto storage_mgr = &self->storage_mgr;

	// read partitions
	list_foreach(&storage_mgr->list)
	{
		auto storage = list_at(Storage, link);
		engine_recover_storage(self, storage);
	}

	// validate partitions per storage
	list_foreach(&storage_mgr->list)
	{
		auto storage = list_at(Storage, link);
		engine_recover_validate(self, storage);
	}

	// open partitions
	list_foreach(&storage_mgr->list)
	{
		auto storage = list_at(Storage, link);
		list_foreach(&storage->list)
		{
			auto part = list_at(Part, link);
			part_open(part);

			auto ref  = ref_allocate(part->id.min, part->id.max);
			ref_prepare(ref, &self->lock, &self->cond_var, part);

			mapping_add(&self->mapping, &ref->slice);
		}
	}
}
