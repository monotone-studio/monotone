#pragma once

//
// monotone
//
// time-series target
//

typedef struct Source Source;

enum
{
	SOURCE_NAME        = 1,
	SOURCE_PATH        = 2,
	SOURCE_SYNC        = 4,
	SOURCE_CRC         = 8,
	SOURCE_COMPRESSION = 16,
	SOURCE_REFRESH_WM  = 32,
	SOURCE_REGION_SIZE = 64
};

struct Source
{
	Str     name;
	Str     path;
	bool    sync;
	bool    crc;
	int64_t compression;
	int64_t refresh_wm;
	int64_t region_size;
};

static inline Source*
source_allocate(void)
{
	auto self = (Source*)mn_malloc(sizeof(Source));
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
source_free(Source* self)
{
	str_free(&self->name);
	str_free(&self->path);
	mn_free(self);
}

static inline void
source_set_name(Source* self, Str* name)
{
	str_free(&self->name);
	str_copy(&self->name, name);
}

static inline void
source_set_path(Source* self, Str* path)
{
	str_free(&self->path);
	str_copy(&self->path, path);
}

static inline void
source_set_sync(Source* self, bool value)
{
	self->sync = value;
}

static inline void
source_set_crc(Source* self, bool value)
{
	self->crc = value;
}

static inline void
source_set_compression(Source* self, int value)
{
	self->compression = value;
}

static inline void
source_set_refresh_wm(Source* self, int value)
{
	self->refresh_wm = value;
}

static inline void
source_set_region_size(Source* self, int value)
{
	self->region_size = value;
}

static inline Source*
source_copy(Source* self)
{
	auto copy = source_allocate();
	guard(copy_guard, source_free, copy);
	source_set_name(copy, &self->name);
	source_set_path(copy, &self->path);
	source_set_sync(copy, self->sync);
	source_set_crc(copy, self->crc);
	source_set_compression(copy, self->compression);
	source_set_refresh_wm(copy, self->refresh_wm);
	source_set_region_size(copy, self->region_size);
	return unguard(&copy_guard);
}

static inline Source*
source_read(uint8_t** pos)
{
	auto self = source_allocate();
	guard(self_guard, source_free, self);

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
source_write(Source* self, Buf* buf)
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
source_alter(Source* self, Source* alter, int mask)
{
	if (mask & SOURCE_NAME)
		source_set_name(self, &alter->name);

	if (mask & SOURCE_PATH)
		source_set_path(self, &alter->path);

	if (mask & SOURCE_SYNC)
		source_set_sync(self, alter->sync);

	if (mask & SOURCE_CRC)
		source_set_crc(self, alter->crc);

	if (mask & SOURCE_COMPRESSION)
		source_set_compression(self, alter->compression);

	if (mask & SOURCE_REFRESH_WM)
		source_set_refresh_wm(self, alter->refresh_wm);

	if (mask & SOURCE_REGION_SIZE)
		source_set_region_size(self, alter->region_size);
}

static inline void
source_pathfmt(Source* self, char* path, int path_size, char* fmt, ...)
{
	// set storage relative directory
	char relative[PATH_MAX];
	va_list args;
	va_start(args, fmt);
	vsnprintf(relative, sizeof(relative), fmt, args);
	va_end(args);

	// set full storage path
	if (str_empty(&self->path))
	{
		// <base>/<storage_name>/relative
		snprintf(path, path_size, "%s/%.*s/%s", config_directory(),
		         str_size(&self->name),
		         str_of(&self->name), relative);
	} else
	{
		if (*str_of(&self->path) == '/')
		{
			// <absolute_storage_path>/relative
			snprintf(path, path_size, "%.*s/%s", str_size(&self->path),
			         str_of(&self->path), relative);
		} else
		{
			// <base>/<storage_path>/relative
			snprintf(path, path_size, "%s/%.*s", config_directory(),
			         str_size(&self->path),
			         str_of(&self->path));
		}
	}
}