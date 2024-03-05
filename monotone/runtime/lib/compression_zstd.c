
//
// monotone
//
// time-series storage
//

#include <monotone_runtime.h>
#include <monotone_lib.h>
#include <zstd.h>

typedef struct CompressionZstd CompressionZstd;

struct CompressionZstd
{
	Compression id;
	ZSTD_CCtx*  ctx;
};

static Compression*
compression_zstd_create(CompressionIf* iface)
{
	CompressionZstd* self = mn_malloc(sizeof(CompressionZstd));
	self->id.iface = iface;
	self->ctx      = ZSTD_createCCtx();
	if (self->ctx == NULL)
	{
		mn_free(self);
		error("failed to create compression context");
	}
	list_init(&self->id.link);
	return &self->id;
}

static void
compression_zstd_free(Compression* ptr)
{
	auto self = (CompressionZstd*)ptr;
	if (self->ctx)
		ZSTD_freeCCtx(self->ctx);
	mn_free(self);
}

static void
compression_zstd_compress(Compression* ptr, Buf* buf, int level,
                          Buf*              a,
                          Buf*              b)
{
	unused(level);

	auto self = (CompressionZstd*)ptr;
	buf_reserve(buf, buf_size(a) + buf_size(b));

	// a
	ZSTD_outBuffer out;
	out.dst  = buf->position;
	out.size = buf_size_unused(buf);
	out.pos  = 0;

	ZSTD_inBuffer in;
	in.src   = a->start;
	in.size  = buf_size(a);
	in.pos   = 0;

	ssize_t rc;
	rc = ZSTD_compressStream2(self->ctx, &out, &in, ZSTD_e_continue);
	assert(rc == 0);

	// b
	in.src   = b->start;
	in.size  = buf_size(b);
	in.pos   = 0;
	rc = ZSTD_compressStream2(self->ctx, &out, &in, ZSTD_e_end);
	assert(rc == 0);
	buf_advance(buf, out.pos);
}

static void
compression_zstd_decompress(Compression* ptr,
                            Buf*         buf,
                            uint8_t*     data,
                            int          data_size,
                            int          data_size_uncompressed)
{
	unused(ptr);
	buf_reserve(buf, data_size_uncompressed);
	ssize_t rc;
	rc = ZSTD_decompress(buf->start, data_size_uncompressed, data, data_size);
	if (unlikely(ZSTD_isError(rc)))
		error("decompression failed");
	assert(rc == data_size_uncompressed);
}

CompressionIf compression_zstd =
{
	.id         = COMPRESSION_ZSTD,
	.create     = compression_zstd_create,
	.free       = compression_zstd_free,
	.compress   = compression_zstd_compress,
	.decompress = compression_zstd_decompress
};
