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
	Storage* storage;
	TierRef* ref;
};

struct Tier
{
	TierStorage* storages;
	TierConfig*  config;
	List         link;
};

static inline void
tier_free(Tier* self)
{
	if (self->storages)
		mn_free(self->storages);
	if (self->config)
		tier_config_free(self->config);
	mn_free(self);
}

static inline Tier*
tier_allocate(TierConfig* config)
{
	auto self = (Tier*)mn_malloc(sizeof(Tier));
	self->storages = NULL;
	self->config   = NULL;
	list_init(&self->link);
	guard(self_guard, tier_free, self);
	self->config = tier_config_copy(config);
	return unguard(&self_guard);
}
