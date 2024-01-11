
//
// monotone
//
// time-series storage
//

#include <monotone_runtime.h>
#include <monotone_lib.h>
#include <monotone_io.h>
#include <monotone_storage.h>

void
tier_mgr_init(TierMgr* self, StorageMgr* storage_mgr)
{
	self->storage_mgr = storage_mgr;
	self->list_count  = 0;
	list_init(&self->list);
}

void
tier_mgr_free(TierMgr* self)
{
	list_foreach_safe(&self->list)
	{
		auto tier = list_at(Tier, link);
		tier_free(tier);
	}
}

static inline void
tier_mgr_save(TierMgr* self)
{
	Buf buf;
	buf_init(&buf);
	guard(guard, buf_free, &buf);

	// create dump
	encode_array(&buf, self->list_count);
	list_foreach(&self->list)
	{
		auto tier = list_at(Tier, link);
		tier_config_write(tier->config, &buf);
	}

	// update state
	var_data_set_buf(&config()->tiers, &buf);
}

void
tier_mgr_open(TierMgr* self)
{
	auto tiers = &config()->tiers;
	if (! var_data_is_set(tiers))
		return;
	auto pos = var_data_of(tiers);
	if (data_is_null(pos))
		return;

	int count;
	data_read_array(&pos, &count);
	for (int i = 0; i < count; i++)
	{
		// create tier config
		auto config = tier_config_read(&pos);
		guard(guard, tier_config_free, config);

		// create tier
		auto tier = tier_allocate(config);
		list_append(&self->list, &tier->link);
		self->list_count++;

		// find and link storages
		tier_resolve(tier, self->storage_mgr);
	}
}

void
tier_mgr_create(TierMgr* self, TierConfig* config, bool if_not_exists)
{
	auto tier = tier_mgr_find(self, &config->name);
	if (tier)
	{
		if (! if_not_exists)
			error("tier '%.*s': already exists", str_size(&config->name),
			      str_of(&config->name));
		return;
	}
	tier = tier_allocate(config);

	// find and link storages
	Exception e;
	if (try(&e)) {
		tier_resolve(tier, self->storage_mgr);
	}
	if (catch(&e))
	{
		tier_free(tier);
		rethrow();
	}

	list_append(&self->list, &tier->link);
	self->list_count++;

	tier_mgr_save(self);
}

void
tier_mgr_drop(TierMgr* self, Str* name, bool if_exists)
{
	auto tier = tier_mgr_find(self, name);
	if (! tier)
	{
		if (! if_exists)
			error("tier '%.*s': not exists", str_size(name), str_of(name));
		return;
	}

	if (tier->refs > 0)
		error("tier '%.*s': has active dependencies",
		      str_size(name), str_of(name));

	list_unlink(&tier->link);
	self->list_count--;
	assert(self->list_count >= 0);
	tier_free(tier);

	tier_mgr_save(self);
}

Tier*
tier_mgr_find(TierMgr* self, Str* name)
{
	list_foreach(&self->list)
	{
		auto tier = list_at(Tier, link);
		if (str_compare(&tier->config->name, name))
			return tier;
	}
	return NULL;
}
