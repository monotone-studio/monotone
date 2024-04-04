
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
pipeline_init(Pipeline* self, StorageMgr* storage_mgr)
{
	self->storage_mgr = storage_mgr;
	self->list_count  = 0;
	list_init(&self->list);
}

static inline void
pipeline_reset(Pipeline* self)
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
pipeline_free(Pipeline* self)
{
	pipeline_reset(self);
}

static inline void
pipeline_save(Pipeline* self)
{
	Buf buf;
	buf_init(&buf);
	guard(buf_free, &buf);

	// create dump
	encode_array(&buf, self->list_count);
	list_foreach(&self->list)
	{
		auto tier = list_at(Tier, link);
		tier_config_write(tier->config, &buf);
	}

	// update state
	var_data_set_buf(&config()->pipeline, &buf);
}

void
pipeline_open(Pipeline* self)
{
	auto var = &config()->pipeline;
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
		guard(tier_config_free, config);

		// create tier
		auto tier = tier_allocate(config);
		list_append(&self->list, &tier->link);
		self->list_count++;

		// find and link storage
		tier_resolve(tier, self->storage_mgr);
	}
}

bool
pipeline_empty(Pipeline* self)
{
	return !self->list_count;
}

void
pipeline_alter(Pipeline* self, List* configs)
{
	List list;
	list_init(&list);

	Exception e;
	if (enter(&e))
	{
		list_foreach(configs)
		{
			auto config = list_at(TierConfig, link);
			auto tier = tier_allocate(config);
			list_append(&list, &tier->link);
			tier_resolve(tier, self->storage_mgr);
		}
	}
	if (leave(&e))
	{
		list_foreach_safe(&list)
		{
			auto tier = list_at(Tier, link);
			tier_free(tier);
		}
		rethrow();
	}

	pipeline_reset(self);

	list_foreach_safe(&list)
	{
		auto tier = list_at(Tier, link);
		list_init(&tier->link);
		list_append(&self->list, &tier->link);
		self->list_count++;
	}

	pipeline_save(self);
}

void
pipeline_rename(Pipeline* self, Str* storage, Str* storage_new)
{
	list_foreach_safe(&self->list)
	{
		auto tier = list_at(Tier, link);
		if (str_compare(&tier->config->name, storage))
		{
			tier_config_set_name(tier->config, storage_new);
			pipeline_save(self);
			break;
		}
	}
}

void
pipeline_show(Pipeline* self, Buf* buf)
{
	// []
	encode_array(buf, self->list_count);
	list_foreach(&self->list)
	{
		auto tier = list_at(Tier, link);
		tier_config_write(tier->config, buf);
	}
}

Tier*
pipeline_primary(Pipeline* self)
{
	if (pipeline_empty(self))
		return NULL;
	auto first = container_of(self->list.next, Tier, link);
	return first;
}
