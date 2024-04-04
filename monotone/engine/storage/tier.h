#pragma once

//
// monotone
//
// time-series storage
//

typedef struct Tier Tier;

struct Tier
{
	Storage*    storage;
	TierConfig* config;
	List        link;
};

static inline void
tier_free(Tier* self)
{
	if (self->storage)
		storage_unref(self->storage);
	if (self->config)
		tier_config_free(self->config);
	mn_free(self);
}

static inline Tier*
tier_allocate(TierConfig* config)
{
	auto self = (Tier*)mn_malloc(sizeof(Tier));
	self->storage = NULL;
	self->config  = NULL;
	list_init(&self->link);
	guard_as(self_guard, tier_free, self);
	self->config = tier_config_copy(config);
	return unguard(&self_guard);
}

static inline void
tier_resolve(Tier* self, StorageMgr* storage_mgr)
{
	assert(! self->storage);
	auto storage = storage_mgr_find(storage_mgr, &self->config->name);
	if (! storage)
		error("storage '%.*s': not exists", str_size(&self->config->name),
		      str_of(&self->config->name));
	self->storage = storage;
	storage_ref(storage);
}
