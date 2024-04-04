#pragma once

//
// monotone
//
// time-series storage
//

typedef struct CloudConfig CloudConfig;

enum
{
	CLOUD_NAME     = 1 << 0,
	CLOUD_TYPE     = 1 << 1,
	CLOUD_LOGIN    = 1 << 2,
	CLOUD_PASSWORD = 1 << 3,
	CLOUD_URL      = 1 << 4,
	CLOUD_DEBUG    = 1 << 5
};

struct CloudConfig
{
	Str  name;
	Str  type;
	Str  login;
	Str  password;
	Str  url;
	bool debug;
};

static inline CloudConfig*
cloud_config_allocate(void)
{
	auto self = (CloudConfig*)mn_malloc(sizeof(CloudConfig));
	self->debug = false;
	str_init(&self->name);
	str_init(&self->type);
	str_init(&self->login);
	str_init(&self->password);
	str_init(&self->url);
	return self;
}

static inline void
cloud_config_free(CloudConfig* self)
{
	str_free(&self->name);
	str_free(&self->type);
	str_free(&self->login);
	str_free(&self->password);
	str_free(&self->url);
	mn_free(self);
}

static inline void
cloud_config_set_name(CloudConfig* self, Str* value)
{
	str_free(&self->name);
	str_copy(&self->name, value);
}

static inline void
cloud_config_set_type(CloudConfig* self, Str* value)
{
	str_free(&self->type);
	str_copy(&self->type, value);
}

static inline void
cloud_config_set_login(CloudConfig* self, Str* value)
{
	str_free(&self->login);
	str_copy(&self->login, value);
}

static inline void
cloud_config_set_password(CloudConfig* self, Str* value)
{
	str_free(&self->password);
	str_copy(&self->password, value);
}

static inline void
cloud_config_set_url(CloudConfig* self, Str* value)
{
	str_free(&self->url);
	str_copy(&self->url, value);
}

static inline void
cloud_config_set_debug(CloudConfig* self, bool value)
{
	self->debug = value;
}

static inline CloudConfig*
cloud_config_copy(CloudConfig* self)
{
	auto copy = cloud_config_allocate();
	guard_as(copy_guard, cloud_config_free, copy);
	cloud_config_set_name(copy, &self->name);
	cloud_config_set_type(copy, &self->type);
	cloud_config_set_login(copy, &self->login);
	cloud_config_set_password(copy, &self->password);
	cloud_config_set_url(copy, &self->url);
	cloud_config_set_debug(copy, self->debug);
	return unguard(&copy_guard);
}

static inline CloudConfig*
cloud_config_read(uint8_t** pos)
{
	auto self = cloud_config_allocate();
	guard_as(self_guard, cloud_config_free, self);

	// map
	int count;
	data_read_map(pos, &count);

	// name
	data_skip(pos);
	data_read_string_copy(pos, &self->name);

	// type
	data_skip(pos);
	data_read_string_copy(pos, &self->type);

	// login
	data_skip(pos);
	data_read_string_copy(pos, &self->login);

	// password
	data_skip(pos);
	data_read_string_copy(pos, &self->password);

	// url
	data_skip(pos);
	data_read_string_copy(pos, &self->url);

	// debug
	data_skip(pos);
	data_read_bool(pos, &self->debug);
	return unguard(&self_guard);
}

static inline void
cloud_config_write(CloudConfig* self, Buf* buf, bool safe)
{
	// map
	encode_map(buf, 6);

	// name
	encode_raw(buf, "name", 4);
	encode_string(buf, &self->name);

	// type
	encode_raw(buf, "type", 4);
	encode_string(buf, &self->type);

	// login
	encode_raw(buf, "login", 5);
	encode_string(buf, &self->login);

	// password
	if (! safe)
	{
		encode_raw(buf, "password", 8);
		encode_string(buf, &self->password);
	} else
	{
		encode_raw(buf, "password", 8);
		encode_cstr(buf, "(secret)");
	}

	// url
	encode_raw(buf, "url", 3);
	encode_string(buf, &self->url);

	// debug
	encode_raw(buf, "debug", 5);
	encode_bool(buf, self->debug);
}

static inline void
cloud_config_alter(CloudConfig* self, CloudConfig* alter, int mask)
{
	if (mask & CLOUD_NAME)
		cloud_config_set_name(self, &alter->name);

	if (mask & CLOUD_TYPE)
		cloud_config_set_type(self, &alter->type);

	if (mask & CLOUD_LOGIN)
		cloud_config_set_login(self, &alter->login);

	if (mask & CLOUD_PASSWORD)
		cloud_config_set_password(self, &alter->password);

	if (mask & CLOUD_URL)
		cloud_config_set_url(self, &alter->url);

	if (mask & CLOUD_DEBUG)
		cloud_config_set_debug(self, alter->debug);
}
