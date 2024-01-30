#pragma once

//
// monotone
//
// time-series storage
//

typedef struct Str Str;

struct Str
{
	char* pos;
	char* end;
	bool  allocated;
};

static inline void
str_init(Str* self)
{
	self->pos = NULL;
	self->end = NULL;
	self->allocated = false;
}

static inline void
str_free(Str* self)
{
	if (self->allocated)
		mn_free(self->pos);
	str_init(self);
}

static inline void
str_set_allocated(Str* self, char* pos, int size)
{
	self->pos = pos;
	self->end = pos + size;
	self->allocated = true;
}

static inline void
str_set(Str* self, char* pos, int size)
{
	self->pos = pos;
	self->end = pos + size;
	self->allocated = false;
}

static inline void
str_set_cstr(Str* self, const char* cstr)
{
	str_set(self, (char*)cstr, strlen(cstr));
}

static inline void
str_set_str(Str* self, Str* str)
{
	self->pos = str->pos;
	self->end = str->end;
	self->allocated = false;
}

static inline int
str_size(Str* self)
{
	return self->end - self->pos;
}

static inline void
str_advance(Str *self, int size)
{
	self->pos += size;
	assert(self->pos <= self->end);
}

static inline bool
str_empty(Str* self)
{
	return str_size(self) == 0;
}

static inline char*
str_of(Str* self)
{
	return self->pos;
}

static inline uint8_t*
str_u8(Str* self)
{
	return (uint8_t*)self->pos;
}

static inline bool
str_compare_raw(Str* self, const void* string, int size)
{
	return str_size(self) == size && !memcmp(self->pos, string, size);
}

static inline bool
str_compare(Str* self, Str* with)
{
	return str_compare_raw(self, str_of(with), str_size(with));
}

static inline void
str_strndup(Str* str, const void* string, int size)
{
	char* pos = mn_malloc(size + 1);
	memcpy(pos, string, size);
	pos[size] = 0;
	str_set_allocated(str, pos, size);
}

static inline void
str_strdup(Str* str, const char* string)
{
	str_strndup(str, string, strlen(string));
}

static inline void
str_copy(Str* str, Str* src)
{
	str_strndup(str, str_of(src), str_size(src));
}
