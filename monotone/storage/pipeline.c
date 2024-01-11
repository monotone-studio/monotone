
//
// monotone
//
// time-series storage
//

#include <monotone_runtime.h>
#include <monotone_lib.h>
#include <monotone_io.h>
#include <monotone_storage.h>

#if 0
static inline PipelineTier*
pipeline_tier_allocate(PipelineRef* ref, Tier* tier)
{
	auto self = (PipelineTier*)mn_malloc(sizeof(PipelineTier));
	self->ref  = ref;
	self->tier = tier;
	list_init(&self->link);
	return self;
}

static inline void
pipeline_tier_free(PipelineTier* self)
{
	if (self->tier)
		tier_unref(self->tier);
	mn_free(self);
}

static inline void
pipeline_free(Pipeline* self)
{
	list_foreach_safe(&self->list)
	{
		auto tier = list_at(PipelineTier, link);
		pipeline_tier_free(tier);
	}
	if (self->config)
		pipeline_config_free(self->config);
	mn_free(self);
}

static inline Pipeline*
pipeline_allocate(PipelineConfig* config)
{
	auto self = (Pipeline*)mn_malloc(sizeof(Pipeline));
	self->config = NULL;
	list_init(&self->list);
	guard(self_guard, pipeline_free, self);
	self->config = pipeline_config_copy(config);
	return unguard(&self_guard);
}

static inline void
pipeline_resolve(Pipeline* self, TierMgr* tier_mgr)
{
	list_foreach(&self->config->list)
	{
		auto ref = list_at(PipelineRef, link);
		auto tier = tier_mgr_find(tier_mgr, &ref->name);
		if (! tier)
			error("tier '%.*s': not exists", str_size(&ref->name),
			      str_of(&ref->name));

		auto pipeline_tier = pipeline_tier_allocate(ref, tier);
		tier_ref(tier);
		list_append(&self->list, &pipeline_tier->link);
	}
}
#endif
