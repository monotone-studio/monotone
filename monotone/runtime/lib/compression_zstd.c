
//
// monotone.
//
// embeddable cloud-native storage for events
// and time-series data.
//
// Copyright (c) 2023-2024 Dmitry Simonenko
// Copyright (c) 2023-2024 Monotone
//
// Distributed under the terms of GNU LGPL v3 or
// Monotone Commercial License.
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
		error("zstd: failed to create compression context");
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

hot static void
compression_zstd_compress(Compression* ptr, Buf* buf, int level,
                          int          argc,
                          Buf**        argv)
{
	auto self = (CompressionZstd*)ptr;
	ZSTD_CCtx_reset(self->ctx, ZSTD_reset_session_only);
	if (level > 0)
		ZSTD_CCtx_setParameter(self->ctx, ZSTD_c_compressionLevel, level);

	size_t size = 0;
	for (int i = 0; i < argc; i++)
		size += buf_size(argv[i]);
	buf_reserve(buf, size);

	ZSTD_outBuffer out;
	out.dst  = buf->position;
	out.size = buf_size_unused(buf);
	out.pos  = 0;

	for (int i = 0; i < argc; i++)
	{
		ZSTD_inBuffer in;
		in.src   = argv[i]->start;
		in.size  = buf_size(argv[i]);
		in.pos   = 0;

		ssize_t rc;
		rc = ZSTD_compressStream2(self->ctx, &out, &in, ZSTD_e_continue);
		assert(rc == 0);
		unused(rc);
	}

	ZSTD_inBuffer in;
	in.src   = NULL;
	in.size  = 0;
	in.pos   = 0;
	ssize_t rc;
	rc = ZSTD_compressStream2(self->ctx, &out, &in, ZSTD_e_end);
	assert(rc == 0);
	unused(rc);

	buf_advance(buf, out.pos);
}

hot static void
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
		error("zstd: decompression failed");
	assert(rc == data_size_uncompressed);
	buf_advance(buf, data_size_uncompressed);
}

CompressionIf compression_zstd =
{
	.id         = COMPRESSION_ZSTD,
	.create     = compression_zstd_create,
	.free       = compression_zstd_free,
	.compress   = compression_zstd_compress,
	.decompress = compression_zstd_decompress
};
