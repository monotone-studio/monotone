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

typedef struct TierConfig TierConfig;

struct TierConfig
{
	Str     name;
	int64_t partitions;
	int64_t size;
	int64_t events;
	int64_t duration;
	List    link;
};

static inline TierConfig*
tier_config_allocate(void)
{
	auto self = (TierConfig*)mn_malloc(sizeof(TierConfig));
	self->partitions = -1;
	self->size       = -1;
	self->events     = -1;
	self->duration   = -1;
	str_init(&self->name);
	list_init(&self->link);
	return self;
}

static inline void
tier_config_free(TierConfig* self)
{
	str_free(&self->name);
	mn_free(self);
}

static inline void
tier_config_set_name(TierConfig* self, Str* name)
{
	str_free(&self->name);
	str_copy(&self->name, name);
}

static inline void
tier_config_set_partitions(TierConfig* self, int64_t value)
{
	self->partitions = value;
}

static inline void
tier_config_set_size(TierConfig* self, int64_t value)
{
	self->size = value;
}

static inline void
tier_config_set_events(TierConfig* self, int64_t value)
{
	self->events = value;
}

static inline void
tier_config_set_duration(TierConfig* self, int64_t value)
{
	self->duration = value;
}

static inline TierConfig*
tier_config_copy(TierConfig* self)
{
	auto copy = tier_config_allocate();
	guard(tier_config_free, copy);
	tier_config_set_name(copy, &self->name);
	tier_config_set_partitions(copy, self->partitions);
	tier_config_set_size(copy, self->size);
	tier_config_set_events(copy, self->events);
	tier_config_set_duration(copy, self->duration);
	return unguard();
}

static inline TierConfig*
tier_config_read(uint8_t** pos)
{
	auto self = tier_config_allocate();
	guard(tier_config_free, self);
	Decode map[] =
	{
		{ DECODE_STRING, "name",       &self->name       },
		{ DECODE_INT,    "partitions", &self->partitions },
		{ DECODE_INT,    "size",       &self->size       },
		{ DECODE_INT,    "events",     &self->events     },
		{ DECODE_INT,    "duration",   &self->duration   },
		{ 0,              NULL,        NULL              },
	};
	decode_map(map, pos);
	return unguard();
}

static inline void
tier_config_write(TierConfig* self, Buf* buf)
{
	// map
	encode_map(buf, 5);

	// name
	encode_raw(buf, "name", 4);
	encode_string(buf, &self->name);

	// partitions
	encode_raw(buf, "partitions", 10);
	encode_integer(buf, self->partitions);

	// size
	encode_raw(buf, "size", 4);
	encode_integer(buf, self->size);

	// events
	encode_raw(buf, "events", 6);
	encode_integer(buf, self->events);

	// duration
	encode_raw(buf, "duration", 8);
	encode_integer(buf, self->duration);
}

static inline bool
tier_config_terminal(TierConfig* self)
{
	return self->partitions == -1 &&
	       self->size       == -1 &&
	       self->events     == -1 &&
	       self->duration   == -1;
}
