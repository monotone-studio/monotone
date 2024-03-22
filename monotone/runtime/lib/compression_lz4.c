
//
// monotone
//
// time-series storage
//

#include <monotone_runtime.h>
#include <monotone_lib.h>
#include <lz4.h>
#include <lz4frame.h>

typedef struct CompressionLz4 CompressionLz4;

struct CompressionLz4
{
	Compression id;
	LZ4F_cctx*  cctx;
	LZ4F_dctx*  dctx;
};

static inline void
compression_lz4_error(size_t rc)
{
	error("lz4: operation failed: %s", LZ4F_getErrorName(rc));
}

static Compression*
compression_lz4_create(CompressionIf* iface)
{
	CompressionLz4* self = mn_malloc(sizeof(CompressionLz4));
	self->id.iface = iface;
	self->cctx     = NULL;
	self->dctx     = NULL;
	LZ4F_errorCode_t rc;
	rc = LZ4F_createCompressionContext(&self->cctx, LZ4F_VERSION);
	if (rc != 0)
	{
		mn_free(self);
		error("lz4: failed to create compression context");
	}
	rc = LZ4F_createDecompressionContext(&self->dctx, LZ4F_VERSION);
	if (rc != 0)
	{
		LZ4F_freeCompressionContext(self->cctx);
		mn_free(self);
		error("lz4: failed to create decompression context");
	}
	list_init(&self->id.link);
	return &self->id;
}

static void
compression_lz4_free(Compression* ptr)
{
	auto self = (CompressionLz4*)ptr;
	if (self->cctx)
		LZ4F_freeCompressionContext(self->cctx);
	if (self->dctx)
		LZ4F_freeDecompressionContext(self->dctx);
	mn_free(self);
}

hot static void
compression_lz4_compress(Compression* ptr, Buf* buf, int level,
                         Buf*              a,
                         Buf*              b)
{
	auto self = (CompressionLz4*)ptr;
	(void)level;

	LZ4F_preferences_t prefs = LZ4F_INIT_PREFERENCES;
	if (level > 0)
		prefs.compressionLevel = level;

	// reserve buffer
	size_t size_begin = LZ4F_HEADER_SIZE_MAX;
	size_t size_a     = LZ4F_compressBound(buf_size(a), &prefs);
	size_t size_b     = 0;
	if (b)
		size_b = LZ4F_compressBound(buf_size(b), &prefs);
	size_t size_end   = LZ4F_compressBound(0, &prefs);
	size_t size       = size_begin + size_a + size_b + size_end;
	buf_reserve(buf, size);

	// begin
	size = LZ4F_compressBegin(self->cctx, buf->position, size_begin, &prefs);
	if (unlikely(LZ4F_isError(size)))
		compression_lz4_error(size);
	buf_advance(buf, size);

	// a
	size = LZ4F_compressUpdate(self->cctx, buf->position, size_a,
	                           a->start, buf_size(a),
	                           NULL);
	if (unlikely(LZ4F_isError(size)))
		compression_lz4_error(size);
	buf_advance(buf, size);

	// b
	if (b)
	{
		size = LZ4F_compressUpdate(self->cctx, buf->position, size_b,
		                           b->start, buf_size(b),
		                           NULL);
		if (unlikely(LZ4F_isError(size)))
			compression_lz4_error(size);
		buf_advance(buf, size);
	}

	// end
	size = LZ4F_compressEnd(self->cctx, buf->position, size_end, NULL);
	if (unlikely(LZ4F_isError(size)))
		compression_lz4_error(size);
	buf_advance(buf, size);
}

hot static void
compression_lz4_decompress(Compression* ptr,
                           Buf*         buf,
                           uint8_t*     data,
                           int          data_size,
                           int          data_size_uncompressed)
{
	auto self = (CompressionLz4*)ptr;
	LZ4F_resetDecompressionContext(self->dctx);

	buf_reserve(buf, data_size_uncompressed);
	int pos = 0;
	while (pos < data_size)
	{
		size_t dst_size = buf_size_unused(buf);
		size_t src_size = data_size - pos;
		LZ4F_errorCode_t rc;
		rc = LZ4F_decompress(self->dctx, buf->position, &dst_size,
		                     data + pos, &src_size,
		                     NULL);
		if (unlikely(LZ4F_isError(rc)))
			compression_lz4_error(rc);
		buf_advance(buf, dst_size);
		pos += src_size;
	}
	assert(buf_size(buf) == data_size_uncompressed);
}

CompressionIf compression_lz4 =
{
	.id         = COMPRESSION_LZ4,
	.create     = compression_lz4_create,
	.free       = compression_lz4_free,
	.compress   = compression_lz4_compress,
	.decompress = compression_lz4_decompress
};
