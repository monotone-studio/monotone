#pragma once

//
// monotone
//
// time-series storage
//

typedef struct Pipeline Pipeline;

struct Pipeline
{
	List        list;
	int         list_count;
	StorageMgr* storage_mgr;
};

void  pipeline_init(Pipeline*, StorageMgr*);
void  pipeline_free(Pipeline*);
void  pipeline_open(Pipeline*);
bool  pipeline_empty(Pipeline*);
void  pipeline_alter(Pipeline*, List*);
void  pipeline_rename(Pipeline*, Str*, Str*);
void  pipeline_print(Pipeline*, Buf*);
Tier* pipeline_primary(Pipeline*);
