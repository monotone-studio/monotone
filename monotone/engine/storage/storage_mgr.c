
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
	guard(buf_free, &buf);

	// create dump
	encode_array(&buf, self->list_count);
	list_foreach(&self->list)
	{
		auto storage = list_at(Storage, link);
		source_write(storage->source, &buf, false, false);
	}

	// update state
	var_data_set_buf(&config()->storages, &buf);
}

static Storage*
storage_mgr_create_object(StorageMgr*, Source*, bool);

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
		guard(source_free, source);

		// create storage
		auto storage = storage_mgr_create_object(self, source, false);
		list_append(&self->list, &storage->link);
		self->list_count++;
	}
}

bool
storage_mgr_create_main(StorageMgr* self)
{
	// create main storage, if not exists
	auto config = source_allocate();
	guard(source_free, config);

	// use instance uuid as main uuid
	Uuid uuid;
	uuid_from_string(&uuid, &config()->uuid.string);
	source_set_uuid(config, &uuid);

	// set name
	Str name;
	str_set_cstr(&name, "main");
	source_set_name(config, &name);
	return storage_mgr_create(self, config, true);
}

static void
storage_set_cloud(StorageMgr* self, Storage* storage, bool attach)
{
	auto source = storage->source;
	if (str_empty(&source->cloud))
		return;

	auto cloud = cloud_mgr_find(self->cloud_mgr, &source->cloud);
	if (! cloud)
		error("storage '%.*s': cloud '%.*s' does not exists",
		      str_size(&source->name),
		      str_of(&source->name),
		      str_size(&source->cloud),
		      str_of(&source->cloud));

	if (attach)
		cloud_attach(cloud, source);

	storage->cloud = cloud;
	cloud_ref(cloud);
}

static Storage*
storage_mgr_create_object(StorageMgr* self, Source* source, bool attach)
{
	auto storage = storage_allocate(source);

	// find cloud object, if defined
	Exception e;
	if (try(&e)) {
		storage_set_cloud(self, storage, attach);
	}
	if (catch(&e))
	{
		storage_free(storage);
		rethrow();
	}
	return storage;
}

static void
storage_mgr_mkdir(Source* source)
{
	// create storage parent directory, if specified
	char path[PATH_MAX];
	if (! str_empty(&source->path))
	{
		if (*str_of(&source->path) == '/')
			snprintf(path, sizeof(path), "%.*s", str_size(&source->path),
			         str_of(&source->path));
		else
			snprintf(path, sizeof(path), "%s/%.*s", config_directory(),
			         str_size(&source->path),
			         str_of(&source->path));
		if (! fs_exists("%s", path))
		{
			log("storage: new directory '%s'", path);
			fs_mkdir(0755, "%s", path);
		}
	}

	// create storage directory, if not exists
	source_pathfmt(source, path, sizeof(path), "");
	if (! fs_exists("%s", path))
	{
		log("storage: new directory '%s'", path);
		fs_mkdir(0755, "%s", path);
	}
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

	// create storage directory
	storage_mgr_mkdir(source);

	// create storage object
	storage = storage_mgr_create_object(self, source, true);
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
		error("storage '%.*s': main storage cannot be dropped",
		      str_size(name), str_of(name));
	}

	auto storage = storage_mgr_find(self, name);
	if (! storage)
	{
		if (! if_exists)
			error("storage '%.*s': not exists", str_size(name),
			      str_of(name));
		return;
	}

	if (storage->refs > 0)
		error("storage '%.*s': has dependencies", str_size(name),
		      str_of(name));

	if (storage->list_count > 0)
		error("storage '%.*s': has partitions", str_size(name),
		      str_of(name));

	if (storage->cloud)
		cloud_detach(storage->cloud, storage->source);

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
	bool is_empty = (storage->list_count == 0);

	if (mask & SOURCE_CRC)
	{
		if (! is_empty)
			error("storage '%.*s': storage is not empty", str_size(&source->name),
			      str_of(&source->name));
	}

	if (mask & SOURCE_PATH)
	{
		if (! is_empty)
			error("storage '%.*s': storage is not empty", str_size(&source->name),
			      str_of(&source->name));

		source_set_uuid(source, &storage->source->uuid);
		storage_mgr_mkdir(source);
	}

	if (mask & SOURCE_ENCRYPTION ||
	    mask & SOURCE_ENCRYPTION_KEY)
	{
		if (! is_empty)
			error("storage '%.*s': storage is not empty", str_size(&source->name),
			      str_of(&source->name));
	}

	// in case of cloud change, ensure all partitions are
	// downloaded first
	if (mask & SOURCE_CLOUD)
	{
		list_foreach(&storage->list)
		{
			auto part = list_at(Part, link);
			if (part_has(part, ID_CLOUD))
				error("storage '%.*s': some partitions are still remain on cloud",
				      str_size(&source->name),
				      str_of(&source->name));
		}

		if (storage->cloud)
			cloud_detach(storage->cloud, storage->source);
	}

	source_alter(storage->source, source, mask);
	storage_mgr_save(self);

	if (mask & SOURCE_CLOUD)
	{
		if (storage->cloud)
		{
			cloud_unref(storage->cloud);
			storage->cloud = NULL;
		}

		list_foreach(&storage->list)
		{
			auto part = list_at(Part, link);
			part->cloud = NULL;
		}

		storage_set_cloud(self, storage, true);

		if (storage->cloud)
		{
			list_foreach(&storage->list)
			{
				auto part = list_at(Part, link);
				part->cloud = storage->cloud;
			}
		}
	}
}

void
storage_mgr_rename(StorageMgr* self, Str* name, Str* name_new, bool if_exists)
{
	if (unlikely(str_compare_raw(name, "main", 4)))
	{
		error("storage '%.*s': main storage cannot be renamed",
		      str_size(name), str_of(name));
	}

	auto storage = storage_mgr_find(self, name);
	if (! storage)
	{
		if (! if_exists)
			error("storage '%.*s': not exists", str_size(name), str_of(name));
		return;
	}

	auto ref = storage_mgr_find(self, name_new);
	if (ref)
		error("storage '%.*s': already exists", str_size(name_new),
		      str_of(name_new));

	source_set_name(storage->source, name_new);
	storage_mgr_save(self);
}

void
storage_mgr_rename_cloud(StorageMgr* self, Str* name, Str* name_new)
{
	list_foreach(&self->list)
	{
		auto storage = list_at(Storage, link);
		if (! str_compare(&storage->source->cloud, name))
			continue;
		source_set_cloud(storage->source, name_new);
	}
	storage_mgr_save(self);
}

void
storage_mgr_show(StorageMgr* self, Buf* buf, bool debug)
{
	// {}
	encode_map(buf, self->list_count);
	list_foreach(&self->list)
	{
		auto storage = list_at(Storage, link);
		encode_string(buf, &storage->source->name);
		storage_show(storage, buf, debug);
	}
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
