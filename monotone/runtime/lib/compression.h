#pragma once

//
// monotone
//
// time-series storage
//

typedef struct CompressionIf CompressionIf;
typedef struct Compression   Compression;

enum
{
	COMPRESSION_NONE = 0,
	COMPRESSION_ZSTD = 1
};

struct CompressionIf
{
	int           id;
	Compression* (*create)(CompressionIf*);
	void         (*free)(Compression*);
	void         (*compress)(Compression*, Buf*, int, Buf*, Buf*);
	void         (*decompress)(Compression*, Buf*, uint8_t*, int, int);
};

struct Compression
{
	CompressionIf* iface;
	List           link;
};

static inline Compression*
compression_create(CompressionIf* iface)
{
	return iface->create(iface);
}

static inline void
compression_free(Compression* self)
{
	return self->iface->free(self);
}

static inline Compression*
compression_of(List* link)
{
	return container_of(link, Compression, link);
}

static inline void
compression_compress(Compression* self, Buf* buf, int level,
                     Buf*         a,
                     Buf*         b)
{
	self->iface->compress(self, buf, level, a, b);
}

static inline void
compression_decompress(Compression* self,
                       Buf*         buf,
                       uint8_t*     data,
                       int          data_size,
                       int          data_size_uncompressed)
{
	self->iface->decompress(self, buf,
	                        data,
	                        data_size,
	                        data_size_uncompressed);
}
