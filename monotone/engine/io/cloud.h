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
	void   (*download)(Cloud*, uint64_t min, uint64_t id);
	void   (*upload)(Cloud*, uint64_t min, uint64_t id);
	void   (*remove)(Cloud*, uint64_t min, uint64_t id);
	void   (*read)(Cloud*, uint64_t min, uint64_t id, Buf*, uint32_t size,
	               uint64_t offset);
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
cloud_download(Cloud* self, uint64_t min, uint64_t id)
{
	self->iface->download(self, min, id);
}

static inline void
cloud_upload(Cloud* self, uint64_t min, uint64_t id)
{
	self->iface->upload(self, min, id);
}

static inline void
cloud_remove(Cloud* self, uint64_t min, uint64_t id)
{
	self->iface->remove(self, min, id);
}

static inline void
cloud_read(Cloud* self, uint64_t min, uint64_t id, Buf* buf,
           uint32_t size,
           uint64_t offset)
{
	self->iface->read(self, min, id, buf, size, offset);
}
