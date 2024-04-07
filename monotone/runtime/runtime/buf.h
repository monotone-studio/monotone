#pragma once

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
		mn_free(self->start);
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

static inline uint8_t**
buf_reserve(Buf* self, int size)
{
	if (likely(buf_size_unused(self) >= size))
		return &self->position;

	int size_actual = buf_size(self) + size;
	int size_grow = buf_capacity(self)*  2;
	if (unlikely(size_actual > size_grow))
		size_grow = size_actual;

	uint8_t* pointer;
	pointer = mn_realloc(self->start, size_grow);

	self->position = pointer + (self->position - self->start);
	self->end = pointer + size_grow;
	self->start = pointer;
	assert((self->end - self->position) >= size);
	return &self->position;
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
buf_write(Buf* self, void* data, int size)
{
	buf_reserve(self, size);
	buf_append(self, data, size);
}

always_inline hot static inline void
buf_write_str(Buf* self, Str* str)
{
	buf_write(self, str_of(str), str_size(str));
}

static inline void
buf_vprintf(Buf* self, const char* fmt, va_list args)
{
	char tmp[1024];
	int  tmp_len;
	tmp_len = vsnprintf(tmp, sizeof(tmp), fmt, args);
	buf_write(self, tmp, tmp_len);
}

static inline void
buf_printf(Buf* buf, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	buf_vprintf(buf, fmt, args);
	va_end(args);
}
