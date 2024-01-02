#pragma once

//
// monotone
//
// time-series storage
//

typedef struct Storage    Storage;
typedef struct StorageMgr StorageMgr;

struct Storage
{
	Str  name;
	Str  path;
	int  capacity;
	int  compaction_wm;
	bool memory;
	bool drop;
	bool sync;
	bool crc;
	int  compression;
	int  region_size;
	int  order;
	List link;
};

struct StorageMgr
{
	List list;
	int  list_count;
};

static inline void
storage_free(Storage* self)
{
	str_free(&self->name);
	str_free(&self->path);
	mn_free(self);
}

static inline Storage*
storage_allocate(Str* name)
{
	auto self = (Storage*)mn_malloc(sizeof(Storage));
	str_init(&self->name);
	str_init(&self->path);
	guard(free, storage_free, self);

	str_copy(&self->name, name);
	self->order         = 0;
	self->capacity      = 0;
	self->compaction_wm = 40 * 1024 * 1024;
	self->memory        = false;
	self->drop          = false;
	self->sync          = true;
	self->crc           = false;
	self->compression   = COMPRESSION_OFF;
	self->region_size   = 128 * 1024;
	unguard(&free);
	return self;
}

static inline void
storage_mgr_init(StorageMgr* self)
{
	self->list_count = 0;
	list_init(&self->list);
}

static inline void
storage_mgr_free(StorageMgr* self)
{
	list_foreach_safe(&self->list)
	{
		auto storage = list_at(Storage, link);
		storage_free(storage);
	}
	list_init(&self->list);
	self->list_count = 0;
}

static inline void
storage_mgr_add(StorageMgr* self, Storage* storage)
{
	storage->order = self->list_count;
	list_append(&self->list, &storage->link);
	self->list_count++;
}

static inline Storage*
storage_mgr_find(StorageMgr* self, Str* name)
{
	list_foreach(&self->list)
	{
		auto storage = list_at(Storage, link);
		if (str_compare(&storage->name, name))
			return storage;
	}
	return NULL;
}
