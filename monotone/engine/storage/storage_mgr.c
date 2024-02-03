
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

void
storage_mgr_init(StorageMgr* self, CloudMgr* cloud_mgr)
{
	self->list_count = 0;
	list_init(&self->list);
	self->cloud_mgr = cloud_mgr;
}

void
storage_mgr_free(StorageMgr* self)
{
	list_foreach_safe(&self->list)
	{
		auto storage = list_at(Storage, link);
		storage_free(storage);
	}
}

static inline void
storage_mgr_save(StorageMgr* self)
{
	Buf buf;
	buf_init(&buf);
	guard(guard, buf_free, &buf);

	// create dump
	encode_array(&buf, self->list_count);
	list_foreach(&self->list)
	{
		auto storage = list_at(Storage, link);
		source_write(storage->source, &buf);
	}

	// update state
	var_data_set_buf(&config()->storages, &buf);
}

static Storage*
storage_mgr_create_object(StorageMgr*, Source*);

void
storage_mgr_open(StorageMgr* self)
{
	auto storages = &config()->storages;
	if (! var_data_is_set(storages))
		return;
	auto pos = var_data_of(storages);
	if (data_is_null(pos))
		return;

	int count;
	data_read_array(&pos, &count);
	for (int i = 0; i < count; i++)
	{
		// create source
		auto source = source_read(&pos);
		guard(guard, source_free, source);

		// create storage
		auto storage = storage_mgr_create_object(self, source);
		list_append(&self->list, &storage->link);
		self->list_count++;
	}
}

void
storage_mgr_close(StorageMgr* self)
{
	list_foreach(&self->list)
	{
		auto storage = list_at(Storage, link);
		list_foreach_safe(&storage->list)
		{
			auto part = list_at(Part, link);
			part_free(part);
		}
		list_init(&storage->list);
		storage->list_count = 0;
	}
}

bool
storage_mgr_create_system(StorageMgr* self)
{
	// create system storage, if not exists
	auto config = source_allocate();
	guard(guard, source_free, config);
	Str name;
	str_set_cstr(&name, "main");
	source_set_name(config, &name);
	return storage_mgr_create(self, config, true);
}

static Storage*
storage_mgr_create_object(StorageMgr* self, Source* source)
{
	auto storage = storage_allocate(source);

	// find cloud interface and create cloud object, if defined
	Exception e;
	if (try(&e))
	{
		if (! str_empty(&source->cloud))
		{
			auto cloud_if = cloud_mgr_find(self->cloud_mgr, &source->cloud);
			if (! cloud_if)
				error("storage '%.*s': cloud interface '%.*s' does not exists",
				      str_size(&source->name),
				      str_of(&source->name),
				      str_size(&source->cloud),
				      str_of(&source->cloud));

			storage->cloud = cloud_create(cloud_if, storage->source);
		}
	}

	if (catch(&e))
	{
		storage_free(storage);
		rethrow();
	}

	return storage;
}

bool
storage_mgr_create(StorageMgr* self, Source* source, bool if_not_exists)
{
	auto storage = storage_mgr_find(self, &source->name);
	if (storage)
	{
		if (! if_not_exists)
			error("storage '%.*s': already exists", str_size(&source->name),
			      str_of(&source->name));
		return false;
	}

	// create storage directory, if not exists
	char path[PATH_MAX];
	source_pathfmt(source, path, sizeof(path), "");
	if (! fs_exists("%s", path))
	{
		log("storage: new directory '%s'", path);
		fs_mkdir(0755, "%s", path);
	}

	// create storage object
	storage = storage_mgr_create_object(self, source);
	list_append(&self->list, &storage->link);
	self->list_count++;

	storage_mgr_save(self);
	return true;
}

void
storage_mgr_drop(StorageMgr* self, Str* name, bool if_exists)
{
	if (unlikely(str_compare_raw(name, "main", 4)))
	{
		error("storage '%.*s': system storage cannot be dropped",
		      str_size(name), str_of(name));
	}

	auto storage = storage_mgr_find(self, name);
	if (! storage)
	{
		if (! if_exists)
			error("storage '%.*s': not exists", str_size(name), str_of(name));
		return;
	}

	if (storage->refs > 0)
		error("storage '%.*s': has active dependencies",
		      str_size(name), str_of(name));

	if (storage->list_count > 0)
		error("storage '%.*s': has data", str_size(name), str_of(name));

	list_unlink(&storage->link);
	self->list_count--;
	assert(self->list_count >= 0);
	storage_free(storage);

	storage_mgr_save(self);
}

void
storage_mgr_alter(StorageMgr* self, Source* source, int mask, bool if_exists)
{
	auto storage = storage_mgr_find(self, &source->name);
	if (! storage)
	{
		if (! if_exists)
			error("storage '%.*s': not exists", str_size(&source->name),
			      str_of(&source->name));
		return;
	}

	source_alter(storage->source, source, mask);
	storage_mgr_save(self);
}

void
storage_mgr_rename(StorageMgr* self, Str* name, Str* name_new, bool if_exists)
{
	if (unlikely(str_compare_raw(name, "main", 4)))
	{
		error("storage '%.*s': system storage cannot be renamed",
		      str_size(name), str_of(name));
	}

	auto storage = storage_mgr_find(self, name_new);
	if (storage)
		error("storage '%.*s': already exists", str_size(name_new),
		      str_of(name_new));

	storage = storage_mgr_find(self, name);
	if (! storage)
	{
		if (! if_exists)
			error("storage '%.*s': not exists", str_size(name), str_of(name));
		return;
	}

	source_set_name(storage->source, name_new);
	storage_mgr_save(self);
}

void
storage_mgr_show(StorageMgr* self, Str* name, Buf* buf)
{
	if (name == NULL)
	{
		list_foreach(&self->list)
		{
			auto storage = list_at(Storage, link);
			storage_stats_show(storage, buf);
		}
		return;
	}

	auto storage = storage_mgr_find(self, name);
	if (! storage)
	{
		error("storage '%.*s': not exists", str_size(name), str_of(name));
		return;
	}
	storage_stats_show(storage, buf);
}

void
storage_mgr_show_partitions(StorageMgr* self, Str* name, Buf* buf)
{
	if (name == NULL)
	{
		list_foreach(&self->list)
		{
			auto storage = list_at(Storage, link);
			storage_show_partitions(storage, buf);
		}
		return;
	}

	auto storage = storage_mgr_find(self, name);
	if (! storage)
	{
		error("storage '%.*s': not exists", str_size(name), str_of(name));
		return;
	}
	storage_show_partitions(storage, buf);
}

Storage*
storage_mgr_find(StorageMgr* self, Str* name)
{
	list_foreach(&self->list)
	{
		auto storage = list_at(Storage, link);
		if (str_compare(&storage->source->name, name))
			return storage;
	}
	return NULL;
}

Part*
storage_mgr_find_part(StorageMgr* self, Storage* exclude, uint64_t min)
{
	list_foreach(&self->list)
	{
		auto storage = list_at(Storage, link);
		if (storage == exclude)
			continue;
		list_foreach(&storage->list)
		{
			auto part = list_at(Part, link);
			if (part->id.min == min)
				return part;
		}
	}
	return NULL;
}
