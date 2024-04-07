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

typedef struct CloudMgr CloudMgr;

struct CloudMgr
{
	List list;
	int  list_count;
};

void cloud_mgr_init(CloudMgr*);
void cloud_mgr_free(CloudMgr*);
void cloud_mgr_open(CloudMgr*);
void cloud_mgr_create(CloudMgr*, CloudConfig*, bool);
void cloud_mgr_drop(CloudMgr*, Str*, bool);
void cloud_mgr_alter(CloudMgr*, CloudConfig*, int, bool);
void cloud_mgr_rename(CloudMgr*, Str*, Str*, bool);
void cloud_mgr_show(CloudMgr*, Str*, Buf*);
Cloud*
cloud_mgr_find(CloudMgr*, Str*);
