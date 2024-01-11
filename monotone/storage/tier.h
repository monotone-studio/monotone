#pragma once

//
// monotone
//
// time-series storage
//

typedef struct TierStorage TierStorage;
typedef struct Tier        Tier;

struct TierStorage
{
	TierRef* ref;
	Storage* storage;
	List     link;
};

struct Tier
{
	List        list;
	int         refs;
	TierConfig* config;
	List        link;
};

static inline TierStorage*
tier_storage_allocate(TierRef* ref, Storage* storage)
{
	auto self = (TierStorage*)mn_malloc(sizeof(TierStorage));
	self->ref = ref;
	self->storage = storage;
	list_init(&self->link);
	return self;
}

static inline void
tier_storage_free(TierStorage* self)
{
	if (self->storage)
		storage_unref(self->storage);
	mn_free(self);
}

static inline void
tier_free(Tier* self)
{
	list_foreach_safe(&self->list)
	{
		auto storage = list_at(TierStorage, link);
		tier_storage_free(storage);
	}
	if (self->config)
		tier_config_free(self->config);
	mn_free(self);
}

static inline Tier*
tier_allocate(TierConfig* config)
{
	auto self = (Tier*)mn_malloc(sizeof(Tier));
	self->refs   = 0;
	self->config = NULL;
	list_init(&self->list);
	list_init(&self->link);
	guard(self_guard, tier_free, self);
	self->config = tier_config_copy(config);
	return unguard(&self_guard);
}

static inline void
tier_ref(Tier* self)
{
	self->refs++;
}

static inline void
tier_unref(Tier* self)
{
	self->refs--;
	assert(self->refs >= 0);
}

static inline void
tier_resolve(Tier* self, StorageMgr* storage_mgr)
{
	assert(list_empty(&self->list));
	if (! self->config->list_count)
		return;

	list_foreach(&self->config->list)
	{
		auto ref = list_at(TierRef, link);
		auto storage = storage_mgr_find(storage_mgr, &ref->name);
		if (! storage)
			error("storage '%.*s': not exists", str_size(&ref->name),
			      str_of(&ref->name));

		auto tier_storage = tier_storage_allocate(ref, storage);
		storage_ref(storage);
		list_append(&self->list, &tier_storage->link);
	}
}
