
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
	compression_cache_init(&self->cache_zstd, &compression_zstd);
}

void
compression_mgr_free(CompressionMgr* self)
{
	compression_cache_free(&self->cache_zstd);
}

Compression*
compression_mgr_pop(CompressionMgr* self, int id)
{
	if (id == COMPRESSION_ZSTD)
		return compression_cache_pop(&self->cache_zstd);
	abort();
	return NULL;
}

void
compression_mgr_push(CompressionMgr* self, Compression* compression)
{
	if (compression->iface->id == COMPRESSION_ZSTD)
	{
		compression_cache_push(&self->cache_zstd, compression);
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
	return -1;
}
