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
	int     list_count;
	List    list;
};

static inline TierConfig*
tier_config_allocate(void)
{
	auto self = (TierConfig*)mn_malloc(sizeof(TierConfig));
	self->capacity   = 0;
	self->list_count = 0;
	list_init(&self->list);
	str_init(&self->name);
	return self;
}

static inline void
tier_config_free(TierConfig* self)
{
	list_foreach_safe(&self->list)
	{
		auto ref = list_at(TierRef, link);
		tier_ref_free(ref);
	}
	str_free(&self->name);
	mn_free(self);
}

static inline TierRef*
tier_config_find(TierConfig* self, Str* name)
{
	list_foreach(&self->list)
	{
		auto ref = list_at(TierRef, link);
		if (str_compare(&ref->name, name))
			return ref;
	}
	return NULL;
}

static inline void
tier_config_add(TierConfig* self, TierRef* ref)
{
	list_append(&self->list, &ref->link);
	self->list_count++;
}

static inline void
tier_config_remove(TierConfig* self, TierRef* ref)
{
	list_unlink(&ref->link);
	self->list_count--;
}

static inline void
tier_config_set_name(TierConfig* self, Str* name)
{
	str_free(&self->name);
	str_copy(&self->name, name);
}

static inline void
tier_config_set_capacity(TierConfig* self, int value)
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
	list_foreach(&self->list)
	{
		auto ref = list_at(TierRef, link);
		auto ref_copy = tier_ref_copy(ref);
		tier_config_add(copy, ref_copy);
	}
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

	// list
	data_skip(pos);
	data_read_array(pos, &count);
	for (int i = 0; i < count; i++)
	{
		auto ref = tier_ref_read(pos);
		tier_config_add(self, ref);
	}
	return unguard(&self_guard);
}

static inline void
tier_config_write(TierConfig* self, Buf* buf)
{
	// map
	encode_map(buf, 3);

	// name
	encode_raw(buf, "name", 4);
	encode_string(buf, &self->name);

	// capacity
	encode_raw(buf, "capacity", 8);
	encode_integer(buf, self->capacity);

	// list
	encode_raw(buf, "list", 3);
	encode_array(buf, self->list_count);
	list_foreach(&self->list)
	{
		auto ref = list_at(TierRef, link);
		tier_ref_write(ref, buf);
	}
}
