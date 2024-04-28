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

typedef struct Source Source;

enum
{
	SOURCE_UUID              = 1 << 0,
	SOURCE_NAME              = 1 << 1,
	SOURCE_PATH              = 1 << 2,
	SOURCE_CLOUD             = 1 << 3,
	SOURCE_CLOUD_DROP_LOCAL  = 1 << 4,
	SOURCE_SYNC              = 1 << 5,
	SOURCE_CRC               = 1 << 6,
	SOURCE_REFRESH_WM        = 1 << 7,
	SOURCE_REGION_SIZE       = 1 << 8,
	SOURCE_COMPRESSION       = 1 << 9,
	SOURCE_COMPRESSION_LEVEL = 1 << 10,
	SOURCE_ENCRYPTION        = 1 << 11,
	SOURCE_ENCRYPTION_KEY    = 1 << 12
};

struct Source
{
	Uuid    uuid;
	Str     name;
	Str     path;
	Str     cloud;
	bool    cloud_drop_local;
	bool    sync;
	bool    crc;
	int64_t refresh_wm;
	int64_t region_size;
	Str     compression;
	int64_t compression_level;
	Str     encryption;
	Str     encryption_key;
};

static inline Source*
source_allocate(void)
{
	auto self = (Source*)mn_malloc(sizeof(Source));
	self->cloud_drop_local  = true;
	self->sync              = true;
	self->crc               = false;
	self->compression_level = 0;
	self->region_size       = 128 * 1024;
	self->refresh_wm        = 40 * 1024 * 1024;
	uuid_init(&self->uuid);
	str_init(&self->name);
	str_init(&self->path);
	str_init(&self->cloud);
	str_init(&self->compression);
	str_init(&self->encryption);
	str_init(&self->encryption_key);
	return self;
}

static inline void
source_free(Source* self)
{
	str_free(&self->name);
	str_free(&self->path);
	str_free(&self->cloud);
	str_free(&self->compression);
	str_free(&self->encryption);
	str_free(&self->encryption_key);
	mn_free(self);
}

static inline void
source_set_uuid(Source* self, Uuid* value)
{
	self->uuid = *value;
}

static inline void
source_set_name(Source* self, Str* value)
{
	str_free(&self->name);
	str_copy(&self->name, value);
}

static inline void
source_set_path(Source* self, Str* value)
{
	str_free(&self->path);
	str_copy(&self->path, value);
}

static inline void
source_set_cloud(Source* self, Str* value)
{
	str_free(&self->cloud);
	str_copy(&self->cloud, value);
}

static inline void
source_set_cloud_drop_local(Source* self, bool value)
{
	self->cloud_drop_local = value;
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
source_set_refresh_wm(Source* self, int value)
{
	self->refresh_wm = value;
}

static inline void
source_set_region_size(Source* self, int value)
{
	self->region_size = value;
}

static inline void
source_set_compression(Source* self, Str* value)
{
	str_free(&self->compression);
	str_copy(&self->compression, value);
}

static inline void
source_set_compression_level(Source* self, int value)
{
	self->compression_level = value;
}

static inline void
source_set_encryption(Source* self, Str* value)
{
	str_free(&self->encryption);
	str_copy(&self->encryption, value);
}

static inline void
source_set_encryption_key(Source* self, Str* value)
{
	str_free(&self->encryption_key);
	str_copy(&self->encryption_key, value);
}

static inline Source*
source_copy(Source* self)
{
	auto copy = source_allocate();
	guard(source_free, copy);
	source_set_uuid(copy, &self->uuid);
	source_set_name(copy, &self->name);
	source_set_path(copy, &self->path);
	source_set_cloud(copy, &self->cloud);
	source_set_cloud_drop_local(copy, self->cloud_drop_local);
	source_set_sync(copy, self->sync);
	source_set_crc(copy, self->crc);
	source_set_refresh_wm(copy, self->refresh_wm);
	source_set_region_size(copy, self->region_size);
	source_set_compression(copy, &self->compression);
	source_set_compression_level(copy, self->compression_level);
	source_set_encryption(copy, &self->encryption);
	source_set_encryption_key(copy, &self->encryption_key);
	return unguard();
}

static inline Source*
source_read(uint8_t** pos)
{
	auto self = source_allocate();
	guard(source_free, self);
	Decode map[] =
	{
		{ DECODE_UUID,   "uuid",              &self->uuid              },
		{ DECODE_STRING, "name",              &self->name              },
		{ DECODE_STRING, "path",              &self->path              },
		{ DECODE_STRING, "cloud",             &self->cloud             },
		{ DECODE_BOOL,   "cloud_drop_local",  &self->cloud_drop_local  },
		{ DECODE_BOOL,   "sync",              &self->sync              },
		{ DECODE_BOOL,   "crc",               &self->crc               },
		{ DECODE_INT,    "refresh_wm",        &self->refresh_wm        },
		{ DECODE_INT,    "region_size",       &self->region_size       },
		{ DECODE_STRING, "compression",       &self->compression       },
		{ DECODE_INT,    "compression_level", &self->compression_level },
		{ DECODE_STRING, "encryption",        &self->encryption        },
		{ DECODE_STRING, "encryption_key",    &self->encryption_key    },
		{ 0,              NULL,               NULL                     },
	};
	decode_map(map, pos);
	return unguard();
}

static inline void
source_write(Source* self, Buf* buf, bool safe, bool debug)
{
	// map
	if (safe)
		encode_map(buf, 12);
	else
		encode_map(buf, 13);

	// uuid
	encode_raw(buf, "uuid", 4);
	if (! debug)
	{
		char uuid[UUID_SZ];
		uuid_to_string(&self->uuid, uuid, sizeof(uuid));
		encode_raw(buf, uuid, sizeof(uuid) - 1);
	} else
	{
		encode_raw(buf, "(filtered)", 10);
	}

	// name
	encode_raw(buf, "name", 4);
	encode_string(buf, &self->name);

	// path
	encode_raw(buf, "path", 4);
	encode_string(buf, &self->path);

	// cloud
	encode_raw(buf, "cloud", 5);
	encode_string(buf, &self->cloud);

	// cloud_drop_local
	encode_raw(buf, "cloud_drop_local", 16);
	encode_bool(buf, self->cloud_drop_local);

	// sync
	encode_raw(buf, "sync", 4);
	encode_bool(buf, self->sync);

	// crc
	encode_raw(buf, "crc", 3);
	encode_bool(buf, self->crc);

	// refresh_wm
	encode_raw(buf, "refresh_wm", 10);
	encode_integer(buf, self->refresh_wm);

	// region_size
	encode_raw(buf, "region_size", 11);
	encode_integer(buf, self->region_size);

	// compression
	encode_raw(buf, "compression", 11);
	encode_string(buf, &self->compression);

	// compression_level
	encode_raw(buf, "compression_level", 17);
	encode_integer(buf, self->compression_level);

	// encryption
	encode_raw(buf, "encryption", 10);
	encode_string(buf, &self->encryption);

	if (! safe)
	{
		// encryption_key
		encode_raw(buf, "encryption_key", 14);
		encode_string(buf, &self->encryption_key);
	}
}

static inline void
source_alter(Source* self, Source* alter, int mask)
{
	if (mask & SOURCE_UUID)
		source_set_uuid(self, &alter->uuid);

	if (mask & SOURCE_NAME)
		source_set_name(self, &alter->name);

	if (mask & SOURCE_PATH)
		source_set_path(self, &alter->path);

	if (mask & SOURCE_CLOUD)
		source_set_cloud(self, &alter->cloud);

	if (mask & SOURCE_CLOUD_DROP_LOCAL)
		source_set_cloud_drop_local(self, alter->cloud_drop_local);

	if (mask & SOURCE_SYNC)
		source_set_sync(self, alter->sync);

	if (mask & SOURCE_CRC)
		source_set_crc(self, alter->crc);

	if (mask & SOURCE_REFRESH_WM)
		source_set_refresh_wm(self, alter->refresh_wm);

	if (mask & SOURCE_REGION_SIZE)
		source_set_region_size(self, alter->region_size);

	if (mask & SOURCE_COMPRESSION)
		source_set_compression(self, &alter->compression);

	if (mask & SOURCE_COMPRESSION_LEVEL)
		source_set_compression_level(self, alter->compression_level);

	if (mask & SOURCE_ENCRYPTION)
		source_set_encryption(self, &alter->encryption);

	if (mask & SOURCE_ENCRYPTION_KEY)
		source_set_encryption_key(self, &alter->encryption_key);
}

static inline void
source_pathfmt(Source* self, char* path, int path_size, char* fmt, ...)
{
	// set storage relative directory
	char relative[512];
	va_list args;
	va_start(args, fmt);
	vsnprintf(relative, sizeof(relative), fmt, args);
	va_end(args);

	char uuid[UUID_SZ];
	uuid_to_string(&self->uuid, uuid, sizeof(uuid));

	// set full storage path
	if (str_empty(&self->path))
	{
		// <base>/<uuid>/relative
		snprintf(path, path_size, "%s/%s/%s", config_directory(), uuid, relative);
	} else
	{
		if (*str_of(&self->path) == '/')
		{
			// <absolute_path>/<uuid>/relative
			snprintf(path, path_size, "%.*s/%s/%s", str_size(&self->path),
			         str_of(&self->path), uuid, relative);
		} else
		{
			// <base>/<path>/<uuid>/relative
			snprintf(path, path_size, "%s/%.*s/%s/%s", config_directory(),
			         str_size(&self->path),
			         str_of(&self->path), uuid, relative);
		}
	}
}
