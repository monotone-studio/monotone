#pragma once

//
// monotone
//
// time-series storage
//

typedef struct PipelineConfig PipelineConfig;

struct PipelineConfig
{
	int  list_count;
	List list;
};

static inline PipelineConfig*
pipeline_config_allocate(void)
{
	auto self = (PipelineConfig*)mn_malloc(sizeof(PipelineConfig));
	self->list_count = 0;
	list_init(&self->list);
	return self;
}

static inline void
pipeline_config_free(PipelineConfig* self)
{
	list_foreach_safe(&self->list)
	{
		auto ref = list_at(PipelineRef, link);
		pipeline_ref_free(ref);
	}
	mn_free(self);
}

static inline PipelineRef*
pipeline_config_find(PipelineConfig* self, Str* name)
{
	list_foreach(&self->list)
	{
		auto ref = list_at(PipelineRef, link);
		if (str_compare(&ref->name, name))
			return ref;
	}
	return NULL;
}

static inline void
pipeline_config_add(PipelineConfig* self, PipelineRef* ref)
{
	list_append(&self->list, &ref->link);
	self->list_count++;
}

static inline void
pipeline_config_remove(PipelineConfig* self, PipelineRef* ref)
{
	list_unlink(&ref->link);
	self->list_count--;
}

static inline PipelineConfig*
pipeline_config_copy(PipelineConfig* self)
{
	auto copy = pipeline_config_allocate();
	guard(copy_guard, pipeline_config_free, copy);
	list_foreach(&self->list)
	{
		auto ref = list_at(PipelineRef, link);
		auto ref_copy = pipeline_ref_copy(ref);
		pipeline_config_add(copy, ref_copy);
	}
	return unguard(&copy_guard);
}

static inline PipelineConfig*
pipeline_config_read(uint8_t** pos)
{
	auto self = pipeline_config_allocate();
	guard(self_guard, pipeline_config_free, self);

	// map
	int count;
	data_read_map(pos, &count);

	// list
	data_skip(pos);
	data_read_array(pos, &count);
	for (int i = 0; i < count; i++)
	{
		auto ref = pipeline_ref_read(pos);
		pipeline_config_add(self, ref);
	}
	return unguard(&self_guard);
}

static inline void
pipeline_config_write(PipelineConfig* self, Buf* buf)
{
	// map
	encode_map(buf, 1);

	// list
	encode_raw(buf, "list", 4);
	encode_array(buf, self->list_count);
	list_foreach(&self->list)
	{
		auto ref = list_at(PipelineRef, link);
		pipeline_ref_write(ref, buf);
	}
}
