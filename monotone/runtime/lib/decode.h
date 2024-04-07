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

typedef struct Decode Decode;

enum
{
	DECODE_UUID   = 1 << 0,
	DECODE_STRING = 1 << 1,
	DECODE_INT    = 1 << 2,
	DECODE_BOOL   = 1 << 3,
	DECODE_REAL   = 1 << 4,
	DECODE_NULL   = 1 << 5,
	DECODE_FOUND  = 1 << 6
};

struct Decode
{
	int         flags;
	const char* key;
	void*       value;
};

static inline void
decode_map(Decode* self, uint8_t** pos)
{
	// read map and compare against keys
	int count;
	data_read_map(pos, &count);
	while (count-- > 0)
	{
		Str key;
		data_read_string(pos, &key);

		bool found = false;
		for (auto ref = self; ref->key; ref++)
		{
			if (! str_compare_cstr(&key, ref->key))
				continue;

			switch (ref->flags & ~DECODE_FOUND) {
			case DECODE_UUID:
			{
				auto value = (Uuid*)ref->value;
				Str uuid;
				data_read_string(pos, &uuid);
				uuid_from_string(value, &uuid);
				break;
			}
			case DECODE_STRING:
			{
				auto value = (Str*)ref->value;
				str_free(value);
				data_read_string_copy(pos, value);
				break;
			}
			case DECODE_INT:
			{
				auto value = (int64_t*)ref->value;
				data_read_integer(pos, value);
				break;
			}
			case DECODE_BOOL:
			{
				auto value = (bool*)ref->value;
				data_read_bool(pos, value);
				break;
			}
			case DECODE_REAL:
			{
				auto value = (double*)ref->value;
				data_read_real(pos, value);
				break;
			}
			case DECODE_NULL:
			{
				data_read_null(pos);
				break;
			}
			default:
				abort();
				break;
			}

			ref->flags |= DECODE_FOUND;
			found = true;
			break;
		}

		if (! found)
		{
			data_skip(pos);
			log("unknown config key '%.*s'", str_size(&key),
			    str_of(&key));
		}
	}

	// ensure all keys were found
	for (auto ref = self; ref->key; ref++)
	{
		if (! (ref->flags & DECODE_FOUND))
			error("config key '%s' is not found", ref->key);
	}
}
