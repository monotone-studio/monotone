#pragma once

//
// noire
//
// time-series storage
//

typedef struct Buf Buf;

struct Buf
{
	uint8_t* start;
	uint8_t* position;
	uint8_t* end;
};

static inline void
buf_init(Buf* self)
{
	self->start    = NULL;
	self->position = NULL;
	self->end      = NULL;
}

static inline void
buf_free(Buf* self)
{
	if (self->start)
		nr_free(self->start);
}

static inline int
buf_capacity(Buf* self)
{
	return self->end - self->start;
}

static inline int
buf_size_unused(Buf* self)
{
	return self->end - self->position;
}

static inline int
buf_size(Buf* self)
{
	return self->position - self->start;
}

static inline void
buf_reset(Buf* self)
{
	self->position = self->start;
}

static inline void
buf_reserve(Buf* self, int size)
{
	if (likely(buf_size_unused(self) >= size))
		return;

	int size_actual = buf_size(self) + size;
	int size_grow = buf_capacity(self)*  2;
	if (unlikely(size_actual > size_grow))
		size_grow = size_actual;

	uint8_t* pointer;
	pointer = nr_realloc(self->start, size_grow);

	self->position = pointer + (self->position - self->start);
	self->end = pointer + size_grow;
	self->start = pointer;
	assert((self->end - self->position) >= size);
}

static inline void
buf_advance(Buf* self, int size)
{
	self->position += size;
	assert(self->position <= self->end);
}

static inline void
buf_truncate(Buf* self, int size)
{
	self->position -= size;
	assert(self->position >= self->start);
}

static inline void
buf_append(Buf* self, void* ptr, int size)
{
	memcpy(self->position, ptr, size);
	buf_advance(self, size);
}

static inline char*
buf_cstr(Buf* self)
{
	return (char*)self->start;
}

static inline uint32_t*
buf_u32(Buf* self)
{
	return (uint32_t*)self->start;
}

static inline uint64_t*
buf_u64(Buf* self)
{
	return (uint64_t*)self->start;
}

static inline void
buf_str(Buf* self, Str* str)
{
	str_set(str, (char*)self->start, buf_size(self));
}

static inline void
buf_write(Buf* buf, void* data, int size)
{
	buf_reserve(buf, size);
	buf_append(buf, data, size);
}

always_inline hot static inline void
buf_write_str(Buf* buf, Str* str)
{
	buf_write(buf, str_of(str), str_size(str));
}
