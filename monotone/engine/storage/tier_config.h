#pragma once

//
// monotone
//
// time-series storage
//

typedef struct TierConfig TierConfig;

struct TierConfig
{
	Str     name;
	int64_t partitions;
	int64_t size;
	int64_t events;
	int64_t interval;
	List    link;
};

static inline TierConfig*
tier_config_allocate(void)
{
	auto self = (TierConfig*)mn_malloc(sizeof(TierConfig));
	self->partitions = INT64_MAX;
	self->size       = INT64_MAX;
	self->events     = INT64_MAX;
	self->interval   = INT64_MAX;
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
tier_config_set_interval(TierConfig* self, int64_t value)
{
	self->interval = value;
}

static inline TierConfig*
tier_config_copy(TierConfig* self)
{
	auto copy = tier_config_allocate();
	guard(copy_guard, tier_config_free, copy);
	tier_config_set_name(copy, &self->name);
	tier_config_set_partitions(copy, self->partitions);
	tier_config_set_size(copy, self->size);
	tier_config_set_events(copy, self->events);
	tier_config_set_interval(copy, self->interval);
	return unguard(&copy_guard);
}

static inline TierConfig*
tier_config_read(uint8_t** pos)
{
	auto self = tier_config_allocate();
	guard(self_guard, tier_config_free, self);

	// map
	int count;
	data_read_map(pos, &count);

	// name
	data_skip(pos);
	data_read_string_copy(pos, &self->name);

	// partitions
	data_skip(pos);
	data_read_integer(pos, &self->partitions);

	// size
	data_skip(pos);
	data_read_integer(pos, &self->size);

	// events
	data_skip(pos);
	data_read_integer(pos, &self->events);

	// interval
	data_skip(pos);
	data_read_integer(pos, &self->interval);
	return unguard(&self_guard);
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

	// interval
	encode_raw(buf, "interval", 8);
	encode_integer(buf, self->interval);
}
