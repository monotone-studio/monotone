
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
conveyor_empty(Conveyor* self)
{
	return !self->list_count;
}

void
conveyor_alter(Conveyor* self, List* configs)
{
	List list;
	list_init(&list);

	Exception e;
	if (try(&e))
	{
		list_foreach(configs)
		{
			auto config = list_at(TierConfig, link);
			auto tier = tier_allocate(config);
			list_append(&list, &tier->link);
			tier_resolve(tier, self->storage_mgr);
		}
	}
	if (catch(&e))
	{
		list_foreach_safe(&list)
		{
			auto tier = list_at(Tier, link);
			tier_free(tier);
		}
		rethrow();
	}

	conveyor_reset(self);

	list_foreach_safe(&list)
	{
		auto tier = list_at(Tier, link);
		list_init(&tier->link);
		list_append(&self->list, &tier->link);
		self->list_count++;
	}

	conveyor_save(self);
}

void
conveyor_rename(Conveyor* self, Str* storage, Str* storage_new)
{
	list_foreach_safe(&self->list)
	{
		auto tier = list_at(Tier, link);
		if (str_compare(&tier->config->name, storage))
		{
			tier_config_set_name(tier->config, storage_new);
			conveyor_save(self);
			break;
		}
	}
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
			buf_printf(buf, "capacity %" PRIi64, tier->config->capacity);
		buf_printf(buf, ")");
		first = false;
	}
	buf_printf(buf, "\n");
}

Tier*
conveyor_primary(Conveyor* self)
{
	if (conveyor_empty(self))
		return NULL;
	auto first = container_of(self->list.next, Tier, link);
	return first;
}
