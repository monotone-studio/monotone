#pragma once

//
// monotone.
//
// embeddable cloud-native storage for events
// and time-series data.
//
// Copyright (c) 2023-2024 Dmitry Simonenko
// Copyright (c) 2023-2024 Monotone
//
// Distributed under the terms of GNU LGPL v3 or
// Monotone Commercial License.
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
void  pipeline_show(Pipeline*, Buf*);
Tier* pipeline_primary(Pipeline*);
