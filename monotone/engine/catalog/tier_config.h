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
	int64_t capacity;
	List    link;
};

static inline TierConfig*
tier_config_allocate(void)
{
	auto self = (TierConfig*)mn_malloc(sizeof(TierConfig));
	self->capacity = INT64_MAX;
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
tier_config_set_capacity(TierConfig* self, int64_t value)
{
	self->capacity = value;
}

static inline TierConfig*
tier_config_copy(TierConfig* self)
{
	auto copy = tier_config_allocate();
	guard(copy_guard, tier_config_free, copy);
	tier_config_set_name(copy, &self->name);
	tier_config_set_capacity(copy, self->capacity);
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

	// capacity
	data_skip(pos);
	data_read_integer(pos, &self->capacity);
	return unguard(&self_guard);
}

static inline void
tier_config_write(TierConfig* self, Buf* buf)
{
	// map
	encode_map(buf, 2);

	// name
	encode_raw(buf, "name", 4);
	encode_string(buf, &self->name);

	// capacity
	encode_raw(buf, "capacity", 8);
	encode_integer(buf, self->capacity);
}
