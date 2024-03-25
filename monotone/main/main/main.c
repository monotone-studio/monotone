
//
// monotone
//
// time-series storage
//

#include <monotone_runtime.h>
#include <monotone_lib.h>
#include <monotone_config.h>
#include <monotone_cloud.h>
#include <monotone_io.h>
#include <monotone_storage.h>
#include <monotone_wal.h>
#include <monotone_engine.h>
#include <monotone_command.h>
#include <monotone_main.h>

static void
main_control_save_config(void* arg)
{
	Main* self = arg;
	char path[PATH_MAX];
	snprintf(path, sizeof(path), "%s/config.json",
	         config_directory());
	config_save(&self->config, path);
}

static void
main_control_lock(void* arg, bool shared)
{
	Main* self = arg;
	rwlock_lock(&self->lock, shared);
}

static void
main_control_unlock(void* arg)
{
	Main* self = arg;
	rwlock_unlock(&self->lock);
}

void
main_init(Main* self)
{
	// control
	auto control = &self->control;
	control->save_config = main_control_save_config;
	control->lock        = main_control_lock;
	control->unlock      = main_control_unlock;
	control->arg         = self;

	// global
	self->global.config          = &self->config;
	self->global.control         = &self->control;
	self->global.memory_mgr      = &self->memory_mgr;
	self->global.compression_mgr = &self->compression_mgr;
	self->global.encryption_mgr  = &self->encryption_mgr;
	self->global.uuid_mgr        = &self->uuid_mgr;

	// runtime
	self->context.log     = (LogFunction)logger_write;
	self->context.log_arg = &self->logger;
	self->context.global  = &self->global;

	comparator_init(&self->comparator);
	logger_init(&self->logger);
	uuid_mgr_init(&self->uuid_mgr);
	memory_mgr_init(&self->memory_mgr, 2 * 1024 * 1024);
	compression_mgr_init(&self->compression_mgr);
	encryption_mgr_init(&self->encryption_mgr);
	config_init(&self->config);
	file_init(&self->lock_directory);

	rwlock_init(&self->lock);
	service_init(&self->service);

	cloud_mgr_init(&self->cloud_mgr);

	wal_init(&self->wal);
	engine_init(&self->engine, &self->comparator,
	            &self->wal,
	            &self->service,
	            &self->cloud_mgr);
	worker_mgr_init(&self->worker_mgr);
}

void
main_free(Main* self)
{
	engine_free(&self->engine);
	wal_free(&self->wal);
	service_free(&self->service);
	cloud_mgr_free(&self->cloud_mgr);
	config_free(&self->config);
	compression_mgr_free(&self->compression_mgr);
	encryption_mgr_free(&self->encryption_mgr);
	memory_mgr_free(&self->memory_mgr);
	rwlock_free(&self->lock);
	file_close(&self->lock_directory);
}

void
main_prepare(Main* self, Compare compare, void* compare_arg)
{
	auto comparator = &self->comparator;
	comparator->compare     = compare;
	comparator->compare_arg = compare_arg;

	// set default configuration
	config_prepare(&self->config);
}

static void
main_deploy(Main* self, Str* directory)
{
	if (config_online())
		log("already open");

	// prepare uuid manager
	uuid_mgr_open(&self->uuid_mgr);

	// generate uuid, unless it is set
	if (! var_string_is_set(&config()->uuid))
	{
		Uuid uuid;
		uuid_mgr_generate(global()->uuid_mgr, &uuid);
		char uuid_sz[UUID_SZ];
		uuid_to_string(&uuid, uuid_sz, sizeof(uuid_sz));
		var_string_set_raw(&config()->uuid, uuid_sz, sizeof(uuid_sz) - 1);
	}

	// set directory
	var_string_set(&config()->directory, directory);

	// create directory if not exists
	auto bootstrap = !fs_exists("%s", str_of(directory));
	if (bootstrap)
		fs_mkdir(0755, "%s", str_of(directory));

	// get exclusive directory lock
	file_open_directory(&self->lock_directory, str_of(directory));
	if (! file_lock(&self->lock_directory))
		error("in use by another process");

	// read or create config file
	char path[PATH_MAX];
	snprintf(path, sizeof(path), "%s/config.json", config_directory());
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

static void
main_replay(Main* self)
{
	WalCursor cursor;
	wal_cursor_init(&cursor);
	guard(cursor_guard, wal_cursor_close, &cursor);

	wal_cursor_open(&cursor, &self->wal, 0);
	uint64_t total = 0;
	for (;;)
	{
		if (! wal_cursor_next(&cursor))
			break;

		// replay operation
		auto write = wal_cursor_at(&cursor);
		switch (write->type) {
		case LOG_WRITE:
			engine_write_replay(&self->engine, write);
			break;
		case LOG_DROP:
		{
			auto drop = (LogDrop*)write;
			engine_drop(&self->engine, drop->id, true, ID|ID_CLOUD);
			break;
		}
		default:
			error("wal: unrecognized operation: %d (lsn: %" PRIu64 ")",
			      write->type, write->lsn);
			break;
		}

		total += write->count;
	}
	log("wal: %.1f million records processed",
	    total / 1000000.0);
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

	// unset comparator for serial mode
	if (config_serial())
		comparator_init(&self->comparator);

	// recover clouds
	cloud_mgr_open(&self->cloud_mgr);

	// recover storages
	engine_open(&self->engine);

	// open wal and replay
	if (var_int_of(&config()->wal_enable))
	{
		wal_open(&self->wal);
		main_replay(self);
	}

	// restore serial number
	if (config_serial())
		engine_set_serial(&self->engine);

	// start compaction workers
	worker_mgr_start(&self->worker_mgr, &self->engine);

	// done
	var_int_set(&config()->online, true);
}

void
main_stop(Main* self)
{
	// shutdown workers
	service_shutdown(&self->service);

	worker_mgr_stop(&self->worker_mgr);

	// close partitions
	engine_close(&self->engine);

	logger_close(&self->logger);
}
