
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
	self->cache = NULL;
}

void
compression_mgr_free(CompressionMgr* self)
{
	if (! self->cache)
		return;
	auto cache = &self->cache[COMPRESSION_ZSTD];
	compression_cache_free(cache);
	mn_free(self->cache);
}

void
compression_mgr_create(CompressionMgr* self)
{
	self->cache = mn_malloc(sizeof(CompressionCache) * 1);
	auto cache = &self->cache[COMPRESSION_ZSTD];
	compression_cache_init(cache, &compression_zstd);
}

Compression*
compression_mgr_pop(CompressionMgr* self, int id)
{
	assert(id != COMPRESSION_NONE);
	return compression_cache_pop(&self->cache[id]);
}

void
compression_mgr_push(CompressionMgr* self, Compression* compression)
{
	auto cache = &self->cache[compression->iface->id];
	compression_cache_push(cache, compression);
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
	return -1;
}
