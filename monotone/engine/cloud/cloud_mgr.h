#pragma once

//
// monotone
//
// time-series storage
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
