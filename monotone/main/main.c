
//
// monotone
//
// time-series storage
//

#include <monotone_runtime.h>
#include <monotone_lib.h>
#include <monotone_config.h>
#include <monotone_main.h>

void
main_init(Main* self)
{
	self->global.config   = &self->config;
	self->global.uuid_mgr = &self->uuid_mgr;
	logger_init(&self->logger);
	uuid_mgr_init(&self->uuid_mgr);
	config_init(&self->config);
}

void
main_free(Main* self)
{
	config_free(&self->config);
}

void
main_prepare(Main* self)
{
	// set default configuration
	config_prepare(&self->config);
}

static void
main_uuid_set(void)
{
	if (! var_string_is_set(&config()->uuid))
	{
		Uuid uuid;
		uuid_mgr_generate(global()->uuid_mgr, &uuid);
		char uuid_sz[UUID_SZ];
		uuid_to_string(&uuid, uuid_sz, sizeof(uuid_sz));
		var_string_set_raw(&config()->uuid, uuid_sz, sizeof(uuid_sz) - 1);
	}
}

static void
main_deploy(Main* self, Str* directory)
{
	// prepare uuid manager
	uuid_mgr_open(&self->uuid_mgr);

	// generate uuid, unless it is set
	main_uuid_set();

	// set directory
	char path[PATH_MAX];
	var_string_set(&config()->directory, directory);

	// create directory if not exists
	auto bootstrap = !fs_exists("%s", str_of(directory));
	if (bootstrap)
		fs_mkdir(0755, "%s", str_of(directory));

	// read or create config file
	snprintf(path, sizeof(path), "%s/config.json",
	         config_directory());
	config_open(&self->config, path);

	// configure logger
	auto logger = &self->logger;
	logger_set_enable(logger, var_int_of(&config()->log_enable));
	logger_set_to_stdout(logger, var_int_of(&config()->log_to_stdout));
	if (var_int_of(&config()->log_to_file))
	{
		snprintf(path, sizeof(path), "%s/log", config_directory());
		logger_open(logger, path);
	}
}

void
main_start(Main* self, const char* directory)
{
	// create base directory and setup logger
	Str path;
	str_init(&path);
	str_set_cstr(&path, directory);
	main_deploy(self, &path);

	// welcome
	log("");
	log("monotone.");
	log("");
	config_print(config());
	log("");

	// recover objects
#if 0
	// recover engine
	engine_open(&self->engine);

	// start compaction
	compaction_mgr_start(&self->compaction_mgr, &self->service, &self->engine);
#endif
}

void
main_stop(Main* self)
{
	(void)self;
}
