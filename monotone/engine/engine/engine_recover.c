
//
// monotone
//
// time-series storage
//

#include <monotone_runtime.h>
#include <monotone_lib.h>
#include <monotone_config.h>
#include <monotone_cloud.h>
#include <monotone_io.h>
#include <monotone_storage.h>
#include <monotone_wal.h>
#include <monotone_engine.h>

static inline int
engine_recover_id(const char* path, uint64_t* id)
{
	// <min>
	// <min>.incomplete
	// <min>.cloud
	// <min>.cloud.incomplete
	*id = 0;
	while (*path && *path != '.')
	{
		if (unlikely(! isdigit(*path)))
			return -1;
		*id = (*id * 10) + *path - '0';
		path++;
	}

	int state = -1;
	if (! *path)
		state = ID;
	else
	if (! strcmp(path, ".incomplete"))
		state = ID_INCOMPLETE;
	else
	if (! strcmp(path, ".complete"))
		state = ID_COMPLETE;
	else
	if (! strcmp(path, ".cloud"))
		state = ID_CLOUD;
	else
	if (! strcmp(path, ".cloud.incomplete"))
		state = ID_CLOUD_INCOMPLETE;
	return state;
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
	guard(closedir, dir);
	for (;;)
	{
		auto entry = readdir(dir);
		if (entry == NULL)
			break;
		if (entry->d_name[0] == '.')
			continue;

		// <min>
		// <min>.incomplete
		// <min>.complete
		// <min>.cloud
		// <min>.cloud.incomplete
		uint64_t min;
		auto state = engine_recover_id(entry->d_name, &min);
		if (state == -1)
		{
			log("storage: skipping unknown file: '%s%s'",
			    path, entry->d_name);
			continue;
		}

		// find or create partition object
		auto part = storage_find(storage, min);
		if (part == NULL)
		{
			Id id =
			{
				.min = min,
				.max = 0
			};
			part = part_allocate(self->comparator, storage->source, &id);
			storage_add(storage, part);
		}
		part_set(part, state);
	}
}

static void
engine_recover_validate(Engine* self, Storage* storage)
{
	list_foreach_safe(&storage->list)
	{
		auto part = list_at(Part, link);

		switch (part->state) {
		case ID:
		case ID | ID_CLOUD:
		case ID_CLOUD:
			// normal states
			break;

		// crash recovery cases (see refresh)

		// crash case 1 (without parent)
		case ID_INCOMPLETE:
			storage_remove(storage, part);
			part_delete(part, ID_INCOMPLETE);
			part_free(part);
			break;

		// crash case 1 (with parent)
		case ID | ID_INCOMPLETE:
			part_delete(part, ID_INCOMPLETE);
			part_unset(part, ID_INCOMPLETE);
			break;

		// crash case 2
		case ID | ID_COMPLETE:
			part_delete(part, ID_COMPLETE);
			part_unset(part, ID_COMPLETE);
			break;

		// crash case 3
		case ID_COMPLETE:
		{
			// crash during refresh on the same storage or move

			// ensure partition does not exists on other storage
			auto ref = storage_mgr_find_part(&self->storage_mgr, storage, part->id.min);
			if (ref)
			{
				if (! part_has(ref, ID))
					error("partition <%" PRIu64"> has unexpected state: %d",
					      ref->id.min, ref->state);

				// parent still exists, remove incomplete partition
				storage_remove(storage, part);
				part_delete(part, ID_COMPLETE);
				part_free(part);
				continue;
			}

			// finilize
			part_rename(part, ID_COMPLETE, ID);
			part_unset(part, ID_COMPLETE);
			part_set(part, ID);
			break;
		}

		// crash case 4
		case ID_CLOUD | ID_INCOMPLETE:
			// crash during download
			//
			// there is no complete state for download, cloud file not
			// removed after download (only prior to refresh)
			part_delete(part, ID_INCOMPLETE);
			part_unset(part, ID_INCOMPLETE);
			break;

		// crash case 5
		case ID | ID_CLOUD_INCOMPLETE:
			// crash during upload
			part_delete(part, ID_CLOUD_INCOMPLETE);
			part_unset(part, ID_CLOUD_INCOMPLETE);
			break;

		default:
			error("partition <%" PRIu64"> has unexpected state: %d",
			      part->id.min, part->state);
			break;
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

			// open partition or cloud file, read index
			if (part_has(part, ID))
				part_open(part, ID, true);
			else
			if (part_has(part, ID_CLOUD))
				part_open(part, ID_CLOUD, true);
			else
				abort();

			// sync metrics
			part->id = part->index.id;
			part_set_time_create(part, part->index.time_create);
			part_set_time_refresh(part, part->index.time_refresh);
			config_lsn_follow(part->index.lsn);
			storage_add_metrics(storage, part);

			// register partition reference
			auto ref = ref_allocate(part->id.min, part->id.max);
			ref_prepare(ref, &self->lock, &self->cond_var, part);
			mapping_add(&self->mapping, &ref->slice);
		}
	}
}
