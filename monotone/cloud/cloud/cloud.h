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
	Cloud* (*create)(CloudIf*, CloudConfig*);
	void   (*free)(Cloud*);
	void   (*attach)(Cloud*, Source*);
	void   (*download)(Cloud*, Source*, Id*);
	void   (*upload)(Cloud*, Source*, Id*);
	void   (*remove)(Cloud*, Source*, Id*);
	void   (*read)(Cloud*, Source*, Id*, Buf*, uint32_t, uint64_t);
};

struct Cloud
{
	int          refs;
	CloudIf*     iface;
	CloudConfig* config;
	List         link;
};

static inline Cloud*
cloud_allocate(CloudIf* iface, CloudConfig* config)
{
	return iface->create(iface, config);
}

static inline void
cloud_free(Cloud* self)
{
	self->iface->free(self);
}

static inline void
cloud_ref(Cloud* self)
{
	self->refs++;
}

static inline void
cloud_unref(Cloud* self)
{
	self->refs--;
}

static inline void
cloud_attach(Cloud* self, Source* source)
{
	self->iface->attach(self, source);
}

static inline void
cloud_download(Cloud* self, Source* source, Id* id)
{
	self->iface->download(self, source, id);
}

static inline void
cloud_upload(Cloud* self, Source* source, Id* id)
{
	self->iface->upload(self, source, id);
}

static inline void
cloud_remove(Cloud* self, Source* source, Id* id)
{
	self->iface->remove(self, source, id);
}

static inline void
cloud_read(Cloud* self, Source* source, Id* id, Buf* buf,
           uint32_t size, uint64_t offset)
{
	self->iface->read(self, source, id, buf, size, offset);
}

static inline void
cloud_show(Cloud* self, Buf* buf)
{
	buf_printf(buf, "%.*s\n", str_size(&self->config->name),
	           str_of(&self->config->name));
	buf_printf(buf, "  type  '%.*s'\n", str_size(&self->config->type),
	           str_of(&self->config->type));
	buf_printf(buf, "  login '%.*s'\n", str_size(&self->config->login),
	           str_of(&self->config->login));
	buf_printf(buf, "  url   '%.*s'\n", str_size(&self->config->url),
	           str_of(&self->config->url));
}
