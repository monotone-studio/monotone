#pragma once

//
// monotone
//
// time-series storage
//

typedef struct Tier    Tier;
typedef struct TierMgr TierMgr;

struct Tier
{
	List     list;
	int      list_count;
	Storage* storage;
};

struct TierMgr
{
	Tier*       tiers;
	int         tiers_count;
	StorageMgr* storage_mgr;
};

static inline void
tier_init(Tier* self, Storage* storage)
{
	self->storage = storage;
	self->list_count = 0;
	list_init(&self->list);
}

static inline void
tier_add(Tier* self, Part* part)
{
	list_append(&self->list, &part->link_tier);
	self->list_count++;
}

static inline void
tier_remove(Tier* self, Part* part)
{
	assert(part->storage == self->storage);
	list_unlink(&part->link_tier);
	self->list_count--;
}

hot static inline Part*
tier_find(Tier* self, uint64_t min)
{
	list_foreach(&self->list)
	{
		auto part = list_at(Part, link_tier);
		if (part->min == min)
			return part;
	}
	return NULL;
}

hot static inline Part*
tier_min(Tier* self)
{
	Part* min = NULL;
	list_foreach(&self->list)
	{
		auto part = list_at(Part, link_tier);
		if (min == NULL)
			min = part;
		else
		if (part->min < min->min)
			min = part;
	}
	return min;
}

static inline Tier*
tier_of(TierMgr* self, int order)
{
	return &self->tiers[order];
}

static inline void
tier_mgr_init(TierMgr* self, StorageMgr* storage_mgr)
{
	self->tiers       = NULL;
	self->tiers_count = 0;
	self->storage_mgr = storage_mgr;
}

static inline void
tier_mgr_free(TierMgr* self)
{
	if (self->tiers)
		mn_free(self->tiers);
}

static inline void
tier_mgr_create(TierMgr* self)
{
	self->tiers_count = self->storage_mgr->list_count;
	self->tiers = mn_malloc(sizeof(Tier) * self->tiers_count);
	list_foreach(&self->storage_mgr->list)
	{
		auto storage = list_at(Storage, link);
		auto tier = tier_of(self, storage->order);
		tier_init(tier, storage);
	}
}
