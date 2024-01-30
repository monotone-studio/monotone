
//
// monotone
//
// time-series storage
//

#include <monotone_runtime.h>
#include <monotone_lib.h>
#include <monotone_config.h>
#include <monotone_io.h>
#include <monotone_catalog.h>

void
storage_mgr_init(StorageMgr* self)
{
	self->list_count = 0;
	list_init(&self->list);
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
		target_write(storage->target, &buf);
	}

	// update state
	var_data_set_buf(&config()->storages, &buf);
}

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
		// create target
		auto target = target_read(&pos);
		guard(guard, target_free, target);

		// create storage
		auto storage = storage_allocate(target);
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

void
storage_mgr_create(StorageMgr* self, Target* target, bool if_not_exists)
{
	auto storage = storage_mgr_find(self, &target->name);
	if (storage)
	{
		if (! if_not_exists)
			error("storage '%.*s': already exists", str_size(&target->name),
			      str_of(&target->name));
		return;
	}

	// create storage directory, if not exists
	auto path = str_of(&target->path);
	if (! fs_exists("%s", path))
	{
		log("storage: new directory '%s'", path);
		fs_mkdir(0755, "%s", path);
	}

	storage = storage_allocate(target);
	list_append(&self->list, &storage->link);
	self->list_count++;

	storage_mgr_save(self);
}

void
storage_mgr_drop(StorageMgr* self, Str* name, bool if_exists)
{
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
storage_mgr_alter(StorageMgr* self, Target* target, int mask, bool if_exists)
{
	auto storage = storage_mgr_find(self, &target->name);
	if (! storage)
	{
		if (! if_exists)
			error("storage '%.*s': not exists", str_size(&target->name),
			      str_of(&target->name));
		return;
	}

	target_alter(storage->target, target, mask);
	storage_mgr_save(self);
}

void
storage_mgr_rename(StorageMgr* self, Str* name, Str* name_new, bool if_exists)
{
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

	target_set_name(storage->target, name_new);
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
		if (str_compare(&storage->target->name, name))
			return storage;
	}
	return NULL;
}

Part*
storage_mgr_find_part(StorageMgr* self, uint64_t id)
{
	list_foreach(&self->list)
	{
		auto storage = list_at(Storage, link);
		list_foreach(&storage->list)
		{
			auto part = list_at(Part, link);
			if (part->id == id)
				return part;
		}
	}
	return NULL;
}
