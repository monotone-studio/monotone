#pragma once

//
// monotone
//
// time-series storage
//

typedef struct PipelineTier PipelineTier;
typedef struct Pipeline     Pipeline;

struct PipelineTier
{
	PipelineRef* ref;
	Tier*        tier;
	List         link;
};

struct Pipeline
{
	List            list;
	PipelineConfig* config;
	List            link;
};

void pipeline_init(Pipeline*);
void pipeline_free(Pipeline*);
void pipeline_open(Pipeline*);
void pipeline_create(Pipeline*, ConveyorConfig*, bool);
void pipeline_drop(Pipeline*, bool);
