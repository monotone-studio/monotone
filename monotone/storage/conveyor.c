
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
conveyor_init(Conveyor* self, StorageMgr* storage_mgr)
{
	self->storage_mgr = storage_mgr;
	self->list_count  = 0;
	list_init(&self->list);
}

static inline void
conveyor_reset(Conveyor* self)
{
	list_foreach_safe(&self->list)
	{
		auto tier = list_at(Tier, link);
		tier_free(tier);
	}
	list_init(&self->list);
	self->list_count = 0;
}

void
conveyor_free(Conveyor* self)
{
	conveyor_reset(self);
}

static inline void
conveyor_save(Conveyor* self)
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
	var_data_set_buf(&config()->conveyor, &buf);
}

void
conveyor_open(Conveyor* self)
{
	auto var = &config()->conveyor;
	if (! var_data_is_set(var))
		return;
	auto pos = var_data_of(var);
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

		// find and link storage
		tier_resolve(tier, self->storage_mgr);
	}
}

bool
conveyor_exists(Conveyor* self)
{
	return self->list_count > 0;
}

void
conveyor_create(Conveyor* self, List* configs, bool if_not_exists)
{
	if (conveyor_exists(self))
	{
		if (! if_not_exists)
			error("conveyor: already exists");
		return;
	}

	Exception e;
	if (try(&e))
	{
		list_foreach(configs)
		{
			auto config = list_at(TierConfig, link);
			auto tier = tier_allocate(config);
			list_append(&self->list, &tier->link);
			self->list_count++;
			tier_resolve(tier, self->storage_mgr);
		}
	}
	if (catch(&e))
	{
		conveyor_reset(self);
		rethrow();
	}

	conveyor_save(self);
}

void
conveyor_drop(Conveyor* self, bool if_exists)
{
	if (! conveyor_exists(self))
	{
		if (! if_exists)
			error("conveyor: not exists");
		return;
	}

	conveyor_reset(self);
	conveyor_save(self);
}

void
conveyor_print(Conveyor* self, Buf* buf)
{
	bool first = true;
	list_foreach(&self->list)
	{
		auto tier = list_at(Tier, link);
		if (! first)
			buf_printf(buf, " ⟶ ");
		buf_printf(buf, "%.*s (", str_size(&tier->config->name),
		           str_of(&tier->config->name));
		if (tier->config->capacity != INT64_MAX)
			buf_printf(buf, "capacity %" PRIu64, tier->config->capacity);
		buf_printf(buf, ")");
		first = false;
	}
}
