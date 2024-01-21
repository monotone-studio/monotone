#pragma once

//
// monotone
//
// time-series target
//

typedef struct Target Target;

enum
{
	TARGET_SYNC        = 1,
	TARGET_CRC         = 2,
	TARGET_COMPRESSION = 4,
	TARGET_REFRESH_WM  = 8,
	TARGET_REGION_SIZE = 16
};

struct Target
{
	Str     name;
	Str     path;
	bool    sync;
	bool    crc;
	int64_t compression;
	int64_t refresh_wm;
	int64_t region_size;
};

static inline Target*
target_allocate(void)
{
	auto self = (Target*)mn_malloc(sizeof(Target));
	self->sync        = true;
	self->crc         = false;
	self->compression = COMPRESSION_OFF;
	self->region_size = 128 * 1024;
	self->refresh_wm  = 40 * 1024 * 1024;
	str_init(&self->name);
	str_init(&self->path);
	return self;
}

static inline void
target_free(Target* self)
{
	str_free(&self->name);
	str_free(&self->path);
	mn_free(self);
}

static inline void
target_set_name(Target* self, Str* name)
{
	str_free(&self->name);
	str_copy(&self->name, name);
}

static inline void
target_set_path(Target* self, Str* path)
{
	str_free(&self->path);
	str_copy(&self->path, path);
}

static inline void
target_set_sync(Target* self, bool value)
{
	self->sync = value;
}

static inline void
target_set_crc(Target* self, bool value)
{
	self->crc = value;
}

static inline void
target_set_compression(Target* self, int value)
{
	self->compression = value;
}

static inline void
target_set_refresh_wm(Target* self, int value)
{
	self->refresh_wm = value;
}

static inline void
target_set_region_size(Target* self, int value)
{
	self->region_size = value;
}

static inline Target*
target_copy(Target* self)
{
	auto copy = target_allocate();
	guard(copy_guard, target_free, copy);
	target_set_name(copy, &self->name);
	target_set_path(copy, &self->path);
	target_set_sync(copy, self->sync);
	target_set_crc(copy, self->crc);
	target_set_compression(copy, self->compression);
	target_set_refresh_wm(copy, self->refresh_wm);
	target_set_region_size(copy, self->region_size);
	return unguard(&copy_guard);
}

static inline Target*
target_read(uint8_t** pos)
{
	auto self = target_allocate();
	guard(self_guard, target_free, self);

	// map
	int count;
	data_read_map(pos, &count);

	// name
	data_skip(pos);
	data_read_string_copy(pos, &self->name);

	// path
	data_skip(pos);
	data_read_string_copy(pos, &self->path);

	// sync
	data_skip(pos);
	data_read_bool(pos, &self->sync);

	// crc
	data_skip(pos);
	data_read_bool(pos, &self->crc);

	// compression
	data_skip(pos);
	data_read_integer(pos, &self->compression);

	// refresh_wm
	data_skip(pos);
	data_read_integer(pos, &self->refresh_wm);

	// region_size
	data_skip(pos);
	data_read_integer(pos, &self->region_size);

	return unguard(&self_guard);
}

static inline void
target_write(Target* self, Buf* buf)
{
	// map
	encode_map(buf, 7);

	// name
	encode_raw(buf, "name", 4);
	encode_string(buf, &self->name);

	// path
	encode_raw(buf, "path", 4);
	encode_string(buf, &self->path);

	// reference
	encode_raw(buf, "sync", 4);
	encode_bool(buf, self->sync);

	// crc
	encode_raw(buf, "crc", 3);
	encode_bool(buf, self->crc);

	// compression
	encode_raw(buf, "compression", 11);
	encode_integer(buf, self->compression);

	// refresh_wm
	encode_raw(buf, "refresh_wm", 10);
	encode_integer(buf, self->refresh_wm);

	// region_size
	encode_raw(buf, "region_size", 11);
	encode_integer(buf, self->region_size);
}

static inline void
target_alter(Target* self, Target* alter, int mask)
{
	if (mask & TARGET_SYNC)
		target_set_sync(self, alter->sync);

	if (mask & TARGET_CRC)
		target_set_crc(self, alter->crc);

	if (mask & TARGET_COMPRESSION)
		target_set_compression(self, alter->compression);

	if (mask & TARGET_REFRESH_WM)
		target_set_compression(self, alter->refresh_wm);

	if (mask & TARGET_REGION_SIZE)
		target_set_region_size(self, alter->region_size);
}
