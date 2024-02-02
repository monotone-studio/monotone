#pragma once

//
// monotone
//
// time-series storage
//

typedef struct CloudIf CloudIf;
typedef struct Cloud   Cloud;

struct CloudIf
{
	Str    name;
	Cloud* (*create)(Source*);
	void   (*free)(Cloud*);
	void   (*download)(Cloud*, Id*);
	void   (*upload)(Cloud*, Id*);
	void   (*remove)(Cloud*, Id*);
	void   (*read)(Cloud*, Id*, Buf*, uint32_t size, uint64_t offset);
	List   link;
};

struct Cloud
{
	CloudIf* iface;
};

static inline void
cloud_iface_init(CloudIf* self, Str* name)
{
	memset(self, 0, sizeof(*self));
	str_set_str(&self->name, name);
	list_init(&self->link);
}

static inline Cloud*
cloud_create(CloudIf* iface, Source* source)
{
	return iface->create(source);
}

static inline void
cloud_free(Cloud* self)
{
	self->iface->free(self);
}

static inline void
cloud_download(Cloud* self, Id* id)
{
	self->iface->download(self, id);
}

static inline void
cloud_upload(Cloud* self, Id* id)
{
	self->iface->upload(self, id);
}

static inline void
cloud_remove(Cloud* self, Id* id)
{
	self->iface->remove(self, id);
}

static inline void
cloud_read(Cloud* self, Id* id, Buf* buf, uint32_t size,
           uint64_t offset)
{
	self->iface->read(self, id, buf, size, offset);
}
