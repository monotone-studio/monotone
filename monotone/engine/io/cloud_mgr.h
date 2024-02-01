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

static inline void
cloud_mgr_init(CloudMgr* self)
{
	self->list_count = 0;
	list_init(&self->list);
}

static inline void
cloud_mgr_register(CloudMgr* self, CloudIf* iface)
{
	list_append(&self->list, &iface->link);
	self->list_count++;
}

static inline CloudIf*
cloud_mgr_find(CloudMgr* self, Str* name)
{
	list_foreach(&self->list)
	{
		auto iface = list_at(CloudIf, link);
		if (str_compare(&iface->name, name))
			return iface;
	}
	return NULL;
}
