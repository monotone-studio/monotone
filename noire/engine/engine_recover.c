
//
// noire
//
// time-series storage
//

#include <noire_runtime.h>
#include <noire_lib.h>
#include <noire_io.h>
#include <noire_engine.h>

#if 0
static inline in_part_t*
in_engine_find_by_id(in_engine_t *engine, uint64_t id)
{
	in_list_t *i;
	in_list_foreach(&engine->level_mgr.list, i)
	{
		in_part_t *part;
		part = in_container_of(i, in_part_t, link);
		if (part->id == id)
			return part;
	}
	return NULL;
}

static inline int
in_part_id_next(char **pointer, uint64_t *id)
{
	char *pos = *pointer;
	*id = 0;
	while (*pos && *pos != '.')
	{
		if (in_unlikely(! isdigit(*pos)))
			return -1;
		*id = (*id * 10) + *pos - '0';
		pos++;
	}
	*pointer = pos;
	return 0;
}

static int
in_part_id_of(char *name, uint64_t *id_origin, uint64_t *id)
{
	/* id_origin */
	char *token = name;
	int rc;
	rc = in_part_id_next(&token, id_origin);
	if (in_unlikely(rc == -1))
		return -1;
	*id = *id_origin;
	if (! *token)
		return 0;

	/* . */
	token++;

	/* id_origin.id */
	rc = in_part_id_next(&token, id);
	if (in_unlikely(rc == -1))
		return -1;
	if (! *token)
		return 0;
	return -1;
}

in_hot static int
in_part_cmp(const void *a_ptr, const void *b_ptr)
{
	in_part_t *a = *(in_part_t**)a_ptr;
	in_part_t *b = *(in_part_t**)b_ptr;
	if (a->id == b->id)
		return 0;
	if (a->id > b->id)
		return 1;
	return -1;
}

void
in_engine_recover_storage_path(in_engine_t *engine, in_storage_t *storage)
{
	/* set storage path */

	/* <storage_path>/<storage_uuid>/<store_uuid> */
	int   storage_path_size;
	char *storage_path;
	if  (storage->path_size == 0)
	{
		in_var_t *var = in_global()->config->directory;
		storage_path_size = var->string_size;
		storage_path = var->string;
	} else
	{
		storage_path_size = storage->path_size;
		storage_path = storage->path;
	}

	char store_sz[37];
	in_uuid_to_string(engine->uuid, store_sz, sizeof(store_sz));

	char path[PATH_MAX];
	in_snprintf(path, PATH_MAX, "%.*s/%s/%s", storage_path_size, storage_path,
	            storage->uuid, store_sz);

	/* create store directory, if not exists */
	if (! in_fs_exists("%s", path))
	{
		in_log("storage: new directory '%s'", path);
		int rc;
		rc = in_fs_mkdir(0755, "%s", path);
		if (in_unlikely(rc == -1))
			in_error("storage: directory '%s' create error", path);
	}
}

static inline void
in_engine_open_storage(in_engine_t *engine, int level, int level_storage)
{
	in_storage_t *storage;
	storage = in_level_storage_of(&engine->level_mgr, level, level_storage)->storage;

	/* set storage path */

	/* <storage_path>/<storage_uuid>/<store_uuid> */
	int   storage_path_size;
	char *storage_path;
	if  (storage->path_size == 0)
	{
		in_var_t *var = in_global()->config->directory;
		storage_path_size = var->string_size;
		storage_path = var->string;
	} else
	{
		storage_path_size = storage->path_size;
		storage_path = storage->path;
	}

	char store_sz[37];
	in_uuid_to_string(engine->uuid, store_sz, sizeof(store_sz));

	char path[PATH_MAX];
	in_snprintf(path, PATH_MAX, "%.*s/%s/%s", storage_path_size, storage_path,
	            storage->uuid, store_sz);

	/* create store directory, if not exists */
	if (! in_fs_exists("%s", path))
	{
		in_log("storage: new directory '%s'", path);
		int rc;
		rc = in_fs_mkdir(0755, "%s", path);
		if (in_unlikely(rc == -1))
			in_error("storage: directory '%s' create error", path);
		return;
	}

	/* read store directory */
	DIR *dir;
	dir = opendir(path);
	if (in_unlikely(dir == NULL))
		in_error("engine: directory '%s' open error", path);

	in_buf_t list;
	in_buf_init(&list);
	in_try
	{
		struct dirent *dir_entry;
		while ((dir_entry = readdir(dir)))
		{
			if (in_unlikely(dir_entry->d_name[0] == '.'))
				continue;

			/* get file id */
			uint64_t id_origin = 0;
			uint64_t id = 0;
			int rc;
			rc = in_part_id_of(dir_entry->d_name, &id_origin, &id);
			if (in_unlikely(rc == -1))
			{
				in_log("engine: skip unknown file '%s/%s'", path, dir_entry->d_name);
				continue;
			}

			/* add partition to the list */
			in_part_t *part;
			part = in_part_allocate(engine->schema, engine->uuid, id_origin, id);
			in_buf_write(&list, &part, sizeof(in_part_t**));
		}

	} in_catch
	{
		in_part_t **part = (in_part_t**)list.start;
		int count = in_buf_count(&list, sizeof(in_part_t*));
		int i = 0;
		for (; i < count; i++)
			in_part_free(part[i]);
		in_buf_unref(&list);
		closedir(dir);
		in_rethrow();
	}
	closedir(dir);

	/* sort partitions */
	int count = in_buf_count(&list, sizeof(in_part_t*));
	qsort(list.start, count, sizeof(in_part_t*), in_part_cmp);

	/* add partitions */
	in_part_t **part = (in_part_t**)list.start;
	int i = 0;
	for (; i < count; i++)
	{
		in_part_set_storage(part[i], storage);
		in_level_mgr_add(&engine->level_mgr, level, level_storage, part[i]);
	}
	in_buf_unref(&list);
}

static inline void
in_engine_validate(in_engine_t *engine)
{
	/* validate and recover partitions */
	in_list_t *i, *n;
	in_list_foreach_safe(&engine->level_mgr.list, i, n)
	{
		in_part_t *part;
		part = in_container_of(i, in_part_t, link);

		/* incomplete drop operation */
		if (part->id == UINT64_MAX)
		{
			/* id_origin.uint64_max */

			/* find and remove original partition */
			in_part_t *origin;
			origin = in_engine_find_by_id(engine, part->id_origin);
			if (in_unlikely(origin))
			{
				in_level_mgr_remove(&engine->level_mgr, origin);
				in_part_free(origin);
			}
			in_level_mgr_remove(&engine->level_mgr, part);

			in_part_drop_delete(part->storage, engine->uuid, part->id_origin);
			in_part_free(part);
			continue;
		}

		/* sync psn */
		in_config_psn_follow(in_global()->config, part->id);
		in_config_psn_follow(in_global()->config, part->id_origin);

		/* incomplete split operation */
		if (part->id != part->id_origin)
		{
			/* id_origin.id */

			/* remove if origin still exists */
			in_part_t *origin;
			origin = in_engine_find_by_id(engine, part->id_origin);
			if (in_unlikely(origin))
			{
				/* remove incomplete partition file */
				in_level_mgr_remove(&engine->level_mgr, part);

				in_part_delete(part);
				in_part_free(part);
				continue;
			}

			/* origin has been removed after split files were created */

			/* rename to completion */
			in_part_rename(part);
			part->id_origin = part->id;
		}
	}

	/* open partitions */
	in_list_foreach(&engine->level_mgr.list, i)
	{
		in_part_t *part;
		part = in_container_of(i, in_part_t, link);

		in_level_t *level;
		level = in_level_of(&engine->level_mgr, part->level);

		/* open and load in-memory, if necessary */
		in_part_open(part, level->tier->config->crc);
		if (level->tier->config->in_memory)
			in_part_load(part);
	}
}

in_part_t*
in_engine_bootstrap(in_engine_t *engine, in_level_t *level)
{
	uint64_t id = in_config_psn_next(in_global()->config);
	in_writer_t writer;
	in_writer_init(&writer);
	in_part_t *part;
	in_try
	{
		/* create new partition with empty range */
		in_tier_config_t *config = level->tier->config;
		in_writer_start(&writer, engine->schema,
		                engine->uuid,
		                id,
		                id,
		                config->compression,
		                config->crc,
		                0);
		in_writer_stop(&writer, 0);

		/* create partition file */
		part = writer.part;

		if (config->snapshot)
		{
			in_part_set_storage(part, level->storage[0]->storage);
			in_part_create(part);
			in_part_rename(part);
			if (! config->in_memory)
				in_part_unload(part);
		}

	} in_catch
	{
		in_writer_free(&writer);
		in_rethrow();
	}
	writer.part = NULL;
	in_writer_free(&writer);
	return part;
}

void
in_engine_recover(in_engine_t *engine)
{
	/* read partitions per storage */
	int order = 0;
	for (; order < engine->level_mgr.level_count; order++)
	{
		in_level_t *level = in_level_of(&engine->level_mgr, order);
		int storage_order = 0;
		for (; storage_order < level->storage_count; storage_order++)
			in_engine_open_storage(engine, order, storage_order);
	}

	/* validate state and open partitions */
	in_engine_validate(engine);

	/* create first partition, if necessary */
	if (in_list_empty(&engine->level_mgr.list))
	{
		in_level_t *first;
		first = in_level_of(&engine->level_mgr, 0);
		in_part_t *bootstrap;
		bootstrap = in_engine_bootstrap(engine, first);
		in_level_mgr_add(&engine->level_mgr, 0, 0, bootstrap);
	}

	/* import partitions */
	in_list_t *i;
	in_list_foreach(&engine->level_mgr.list, i)
	{
		in_part_t *part;
		part = in_container_of(i, in_part_t, link);
		in_part_tree_add(&engine->tree, part);
	}

	/* link partitions */
	in_rbtree_node_t *node = in_rbtree_min(&engine->tree.tree);
	in_part_t *l = NULL;
	while (node)
	{
		in_part_t *r;
		r = in_container_of(node, in_part_t, node);
		if (l)
			l->r = r;
		r->l = l;
		l = r;
		node = in_rbtree_next(&engine->tree.tree, node);
	}
}
#endif

void
engine_recover(Engine* self)
{
	(void)self;
}
