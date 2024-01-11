#pragma once

//
// monotone
//
// time-series storage
//

typedef struct PipelineRef PipelineRef;

struct PipelineRef
{
	Str  name;
	List link;
};

static inline PipelineRef*
pipeline_ref_allocate(void)
{
	auto self = (PipelineRef*)mn_malloc(sizeof(PipelineRef));
	str_init(&self->name);
	return self;
}

static inline void
pipeline_ref_free(PipelineRef* self)
{
	str_free(&self->name);
	mn_free(self);
}

static inline void
pipeline_ref_set_name(PipelineRef* self, Str* name)
{
	str_free(&self->name);
	str_copy(&self->name, name);
}

static inline PipelineRef*
pipeline_ref_copy(PipelineRef* self)
{
	auto copy = pipeline_ref_allocate();
	guard(copy_guard, pipeline_ref_free, copy);
	pipeline_ref_set_name(copy, &self->name);
	return unguard(&copy_guard);
}

static inline PipelineRef*
pipeline_ref_read(uint8_t** pos)
{
	auto self = pipeline_ref_allocate();
	guard(self_guard, pipeline_ref_free, self);

	// map
	int count;
	data_read_map(pos, &count);

	// name
	data_skip(pos);
	data_read_string_copy(pos, &self->name);
	return unguard(&self_guard);
}

static inline void
pipeline_ref_write(PipelineRef* self, Buf* buf)
{
	// map
	encode_map(buf, 1);

	// name
	encode_raw(buf, "name", 4);
	encode_string(buf, &self->name);
}
