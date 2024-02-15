#pragma once

//
// monotone
//
// time-series storage
//

typedef struct CompressionCache CompressionCache;

struct CompressionCache
{
	Spinlock       lock;
	List           list;
	int            list_count;
	CompressionIf* iface;
};

static inline void
compression_cache_init(CompressionCache* self, CompressionIf* iface)
{
	self->iface      = iface;
	self->list_count = 0;
	spinlock_init(&self->lock);
	list_init(&self->list);
}

static inline void
compression_cache_free(CompressionCache* self)
{
	list_foreach_safe(&self->list)
	{
		auto ref = list_at(Compression, link);
		compression_free(ref);
	}
	spinlock_free(&self->lock);
}

static inline Compression*
compression_cache_pop(CompressionCache* self)
{
	spinlock_lock(&self->lock);
	if (likely(self->list_count > 0))
	{
		auto ref = container_of(list_pop(&self->list), Compression, link);
		self->list_count--;
		spinlock_unlock(&self->lock);
		return ref;
	}
	spinlock_unlock(&self->lock);
	return compression_create(self->iface);
}

static inline void
compression_cache_push(CompressionCache* self, Compression* ref)
{
	spinlock_lock(&self->lock);
	list_append(&self->list, &ref->link);
	self->list_count++;
	spinlock_unlock(&self->lock);
}
