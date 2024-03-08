
//
// monotone
//
// time-series storage
//

#include <monotone_runtime.h>
#include <monotone_lib.h>

void
compression_mgr_init(CompressionMgr* self)
{
	cache_init(&self->cache_zstd);
	cache_init(&self->cache_lz4);
}

void
compression_mgr_free(CompressionMgr* self)
{
	list_foreach_safe(&self->cache_zstd.list)
	{
		auto ref = list_at(Compression, link);
		compression_free(ref);
	}
	cache_reset(&self->cache_zstd);
	cache_free(&self->cache_zstd);

	cache_reset(&self->cache_lz4);
	cache_free(&self->cache_lz4);
}

Compression*
compression_mgr_pop(CompressionMgr* self, int id)
{
	if (id == COMPRESSION_ZSTD)
	{
		auto ref = cache_pop(&self->cache_zstd);
		if (likely(ref))
			return compression_of(ref);
		return compression_create(&compression_zstd);
	}
	if (id == COMPRESSION_LZ4)
	{
		auto ref = cache_pop(&self->cache_lz4);
		if (likely(ref))
			return compression_of(ref);
		return compression_create(&compression_lz4);
	}
	return NULL;
}

void
compression_mgr_push(CompressionMgr* self, Compression* compression)
{
	if (compression->iface->id == COMPRESSION_ZSTD)
	{
		cache_push(&self->cache_zstd, &compression->link);
		return;
	}
	if (compression->iface->id == COMPRESSION_LZ4)
	{
		cache_push(&self->cache_zstd, &compression->link);
		return;
	}
	abort();
}

int
compression_mgr_of(Str* name)
{
	if (!name || str_empty(name))
		return COMPRESSION_NONE;
	if (str_compare_raw(name, "none", 4))
		return COMPRESSION_NONE;
	if (str_compare_raw(name, "zstd", 4))
		return COMPRESSION_ZSTD;
	if (str_compare_raw(name, "lz4", 3))
		return COMPRESSION_LZ4;
	return -1;
}
