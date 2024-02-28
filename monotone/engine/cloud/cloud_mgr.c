
//
// monotone
//
// time-series storage
//

#include <monotone_runtime.h>
#include <monotone_lib.h>
#include <monotone_config.h>
#include <monotone_cloud.h>

static CloudIf*
cloud_mgr_interface_of(Str* name)
{
	if (str_compare_raw(name, "mock", 4))
		return &cloud_mock;
	if (str_compare_raw(name, "s3", 2))
		return &cloud_s3;
	return NULL;
}

void
cloud_mgr_init(CloudMgr* self)
{
	self->list_count = 0;
	list_init(&self->list);
}

void
cloud_mgr_free(CloudMgr* self)
{
	list_foreach_safe(&self->list)
	{
		auto cloud = list_at(Cloud, link);
		cloud_free(cloud);
	}
}

static inline void
cloud_mgr_save(CloudMgr* self)
{
	Buf buf;
	buf_init(&buf);
	guard(guard, buf_free, &buf);

	// create dump
	encode_array(&buf, self->list_count);
	list_foreach(&self->list)
	{
		auto cloud = list_at(Cloud, link);
		cloud_config_write(cloud->config, &buf);
	}

	// update state
	var_data_set_buf(&config()->clouds, &buf);
}

static Cloud*
cloud_mgr_create_object(CloudMgr* self, CloudConfig* config)
{
	// find interface
	auto iface = cloud_mgr_interface_of(&config->type);
	if (! iface)
		error("cloud '%.*s': unknown cloud type", str_size(&config->name),
		      str_of(&config->name));

	// create cloud object
	auto cloud = cloud_allocate(iface, config);
	list_append(&self->list, &cloud->link);
	self->list_count++;
	return cloud;
}

void
cloud_mgr_open(CloudMgr* self)
{
	auto clouds = &config()->clouds;
	if (! var_data_is_set(clouds))
		return;
	auto pos = var_data_of(clouds);
	if (data_is_null(pos))
		return;

	int count;
	data_read_array(&pos, &count);
	for (int i = 0; i < count; i++)
	{
		// create cloud config
		auto config = cloud_config_read(&pos);
		guard(guard, cloud_config_free, config);

		// create cloud object
		cloud_mgr_create_object(self, config);
	}
}

void
cloud_mgr_create(CloudMgr* self, CloudConfig* config, bool if_not_exists)
{
	auto cloud = cloud_mgr_find(self, &config->name);
	if (cloud)
	{
		if (! if_not_exists)
			error("cloud '%.*s': already exists", str_size(&config->name),
			      str_of(&config->name));
		return;
	}

	// create cloud object
	cloud_mgr_create_object(self, config);

	// update config
	cloud_mgr_save(self);
}

void
cloud_mgr_drop(CloudMgr* self, Str* name, bool if_exists)
{
	auto cloud = cloud_mgr_find(self, name);
	if (! cloud)
	{
		if (! if_exists)
			error("cloud '%.*s': not exists", str_size(name),
			      str_of(name));
		return;
	}

	if (cloud->refs > 0)
		error("cloud '%.*s': has dependencies", str_size(name),
		      str_of(name));

	list_unlink(&cloud->link);
	self->list_count--;
	assert(self->list_count >= 0);
	cloud_free(cloud);

	cloud_mgr_save(self);
}

void
cloud_mgr_alter(CloudMgr* self, CloudConfig* config, int mask, bool if_exists)
{
	auto cloud = cloud_mgr_find(self, &config->name);
	if (! cloud)
	{
		if (! if_exists)
			error("cloud '%.*s': not exists", str_size(&config->name),
			      str_of(&config->name));
		return;
	}

	// update and save cloud config
	cloud_config_alter(cloud->config, config, mask);
	cloud_mgr_save(self);
}

void
cloud_mgr_rename(CloudMgr* self, Str* name, Str* name_new, bool if_exists)
{
	auto cloud = cloud_mgr_find(self, name);
	if (! cloud)
	{
		if (! if_exists)
			error("cloud '%.*s': not exists", str_size(name),
			      str_of(name));
		return;
	}

	// ensure cloud with the same name already exists
	auto ref = cloud_mgr_find(self, name_new);
	if (ref)
		error("cloud '%.*s': already exists", str_size(name_new),
		      str_of(name_new));

	cloud_config_set_name(cloud->config, name_new);
	cloud_mgr_save(self);
}

void
cloud_mgr_show(CloudMgr* self, Str* name, Buf* buf)
{
	if (name == NULL)
	{
		list_foreach(&self->list)
		{
			auto cloud = list_at(Cloud, link);
			cloud_show(cloud, buf);
		}
		return;
	}

	auto cloud = cloud_mgr_find(self, name);
	if (! cloud)
	{
		error("cloud '%.*s': not exists", str_size(name),
		      str_of(name));
		return;
	}
	cloud_show(cloud, buf);
}

Cloud*
cloud_mgr_find(CloudMgr* self, Str* name)
{
	list_foreach(&self->list)
	{
		auto cloud = list_at(Cloud, link);
		if (str_compare(&cloud->config->name, name))
			return cloud;
	}
	return NULL;
}
