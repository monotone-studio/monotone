
//
// monotone
//
// time-series storage
//

#include <monotone_runtime.h>
#include <monotone_lib.h>
#include <monotone_io.h>
#include <monotone_engine.h>

enum
{
	PART_FILE            = 1,
	PART_FILE_INCOMPLETE = 2
};

static inline int64_t
engine_recover_id(const char* path, int* state)
{
	// <id>
	// <id>.incomplete
	int64_t id = 0;
	while (*path && *path != '.')
	{
		if (unlikely(! isdigit(*path)))
			return -1;
		id = (id * 10) + *path - '0';
		path++;
	}

	if (! *path)
		*state = PART_FILE;
	else
	if (! strcmp(path, ".incomplete"))
		*state = PART_FILE_INCOMPLETE;
	else
		return -1;

	return id;
}

static void
engine_tier_read(Engine* self, Tier* tier)
{
	auto storage = tier->storage;
	if (storage->capacity == 0)
		return;

	// create storage directory, if not exists
	const char* path = str_of(&storage->path);
	if (! fs_exists("%s", path))
	{
		log("storage: new directory '%s'", path);
		fs_mkdir(0755, "%s", path);
		return;
	}

	// read directory
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

		int state = 0;
		int64_t id = engine_recover_id(entry->d_name, &state);
		if (id == -1)
			continue;

		// find or create partition
		auto part = tier_find(tier, id);
		if (part) {
			part->state |= state;
		} else
		{
			part = part_allocate(self->comparator, tier->storage,
			                     id,
			                     id + config()->interval);
			part->state   = state;
			part->storage = tier->storage;
			tier_add(tier, part);
		}
	}
}

static void
engine_tier_resolve(Engine* self, Tier* tier, Part* part)
{
	auto next_tier = tier_of(&self->tier_mgr, tier->storage->order + 1);
	auto next = tier_find(next_tier, part->min);
	if (next == NULL)
		return;

	// incomplete partitions should be possible only
	// on the next tier
	//

	// remove incomplete partition
	tier_remove(next_tier, next);
	part_delete(next, false);
	part_free(next);
}

static void
engine_tier_recover(Engine* self, Tier* tier)
{
	list_foreach(&tier->list)
	{
		auto part = list_at(Part, link_tier);

		// resolve cross tier states
		if (tier->storage->order < (self->tier_mgr.tiers_count - 1))
			engine_tier_resolve(self, tier, part);

		switch (part->state) {
		case PART_FILE:
			// normal state
			break;

		// crash recovery cases
		case PART_FILE | PART_FILE_INCOMPLETE:
			// remove incomplete
			part_delete(part, false);
			break;

		case PART_FILE_INCOMPLETE:
			// rename
			part_rename(part);
			break;
		}

		// open partition
		part_open(part, tier->storage->crc);

		// add to the partition list and tree
		list_append(&self->list, &part->link);
		self->list_count++;

		part_tree_add(&self->tree, part);
	}
}

void
engine_recover(Engine* self)
{
	// read partitions per storage
	for (int order = 0; order < self->tier_mgr.tiers_count; order++)
	{
		auto tier = tier_of(&self->tier_mgr, order);
		auto storage = tier->storage;
		if (str_empty(&storage->path))
			continue;
		engine_tier_read(self, tier);
		engine_tier_recover(self, tier);
	}
}
