#pragma once

//
// monotone
//
// time-series storage
//

typedef struct TierRef TierRef;

struct TierRef
{
	Str  name;
	List link;
};

static inline TierRef*
tier_ref_allocate(void)
{
	auto self = (TierRef*)mn_malloc(sizeof(TierRef));
	str_init(&self->name);
	return self;
}

static inline void
tier_ref_free(TierRef* self)
{
	str_free(&self->name);
	mn_free(self);
}

static inline void
tier_ref_set_name(TierRef* self, Str* name)
{
	str_free(&self->name);
	str_copy(&self->name, name);
}

static inline TierRef*
tier_ref_copy(TierRef* self)
{
	auto copy = tier_ref_allocate();
	guard(copy_guard, tier_ref_free, copy);
	tier_ref_set_name(copy, &self->name);
	return unguard(&copy_guard);
}

static inline TierRef*
tier_ref_read(uint8_t** pos)
{
	auto self = tier_ref_allocate();
	guard(self_guard, tier_ref_free, self);

	// map
	int count;
	data_read_map(pos, &count);

	// name
	data_skip(pos);
	data_read_string_copy(pos, &self->name);
	return unguard(&self_guard);
}

static inline void
tier_ref_write(TierRef* self, Buf* buf)
{
	// map
	encode_map(buf, 1);

	// name
	encode_raw(buf, "name", 4);
	encode_string(buf, &self->name);
}
