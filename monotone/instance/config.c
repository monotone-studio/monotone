
//
// monotone
//
// time-series storage
//

#include <monotone_runtime.h>
#include <monotone_lib.h>
#include <monotone_io.h>
#include <monotone_engine.h>
#include <monotone_instance.h>

static inline void
instance_config_u64(Lex* lex, Token* name, uint64_t* value)
{
	// [=]
	lex_if(lex, '=', NULL);

	// int
	Token tk;
	if (! lex_if(lex, TINT, &tk))
		error("config: %.*s = <integer> expected", str_size(&name->string),
		      str_of(&name->string));

	*value = tk.integer;
}

static inline void
instance_config_int(Lex* lex, Token* name, int* value)
{
	// [=]
	lex_if(lex, '=', NULL);

	// int
	Token tk;
	if (! lex_if(lex, TINT, &tk))
		error("config: %.*s = <integer> expected", str_size(&name->string),
		      str_of(&name->string));

	*value = tk.integer;
}

static inline void
instance_config_bool(Lex* lex, Token* name, bool* value)
{
	// [=]
	lex_if(lex, '=', NULL);

	// [true/false]
	Token tk;
	if (! lex_if(lex, TNAME, &tk))
	{
		*value = true;
		return;
	}
	if (str_compare_raw(&tk.string, "true", 4))
		*value = true;
	else
	if (str_compare_raw(&tk.string, "false", 5))
		*value = false;
	else
		error("config: %.*s = <true/false> expected", str_size(&name->string),
		      str_of(&name->string));
}

static inline void
instance_config_string(Lex* lex, Token* name, Str* value)
{
	// [=]
	lex_if(lex, '=', NULL);

	// string
	Token tk;
	if (! lex_if(lex, TSTRING, &tk))
		error("config: %.*s = <string> expected", str_size(&name->string),
		      str_of(&name->string));
	
	str_free(value);
	str_copy(value, &tk.string);
}

static inline void
instance_config_storage(Instance* self, Lex* lex)
{
	// name (options)
	Token name_storage;
	if (! lex_if(lex, TNAME, &name_storage))
		error("config: storage <name> expected");

	// ensure storage does not exists
	auto storage = storage_mgr_find(&self->storage_mgr, &name_storage.string);
	if (storage)
		error("config: storage <%.*s> redefined", str_size(&name_storage.string),
		      str_of(&name_storage.string));

	// create storage
	storage = storage_allocate(&name_storage.string);
	storage_mgr_add(&self->storage_mgr, storage);

	// (
	if (! lex_if(lex, '(', NULL))
		error("config: storage name <(> expected");

	// [)]
	if (lex_if(lex, ')', NULL))
		return;

	for (;;)
	{
		// key [= value]
		Token name;
		if (! lex_if(lex, TNAME, &name))
			error("config: <name> expected");

		if (str_compare_raw(&name.string, "path", 4))
		{
			// path <string>
			instance_config_string(lex, &name, &storage->path);
		} else
		if (str_compare_raw(&name.string, "capacity", 8))
		{
			// capacity <int>
			instance_config_int(lex, &name, &storage->capacity);
		} else
		if (str_compare_raw(&name.string, "compaction_wm", 13))
		{
			// compaction_wm <int>
			instance_config_int(lex, &name, &storage->compaction_wm);
		} else
		if (str_compare_raw(&name.string, "in_memory", 9))
		{
			// in_memory <bool>
			instance_config_bool(lex, &name, &storage->memory);
		} else
		if (str_compare_raw(&name.string, "sync", 4))
		{
			// sync <bool>
			instance_config_bool(lex, &name, &storage->sync);
		} else
		if (str_compare_raw(&name.string, "crc", 3))
		{
			// crc <bool>
			instance_config_bool(lex, &name, &storage->crc);
		} else
		if (str_compare_raw(&name.string, "compression", 11))
		{
			// compression <int>
			instance_config_int(lex, &name, &storage->compression);
		} else
		if (str_compare_raw(&name.string, "region_size", 11))
		{
			// region_size <int>
			instance_config_int(lex, &name, &storage->region_size);
		} else
		{
			error("config: unknown storage option %.*s",
			      str_size(&name.string),
			      str_of(&name.string));
		}

		// [,]
		if (lex_if(lex, ',', NULL))
			continue;

		// )
		if (! lex_if(lex, ')', NULL))
			error("config: storage name (<)> unexpected");

		break;
	}

	if (str_empty(&storage->path))
	{
		char path[PATH_MAX];
		snprintf(path, sizeof(path), "%s/%.*s", config_path(),
		         str_size(&name_storage.string),
		         str_of(&name_storage.string));
		str_strdup(&storage->path, path);
	}
}

void
instance_config(Instance* self, const char* options)
{
	Str text;
	str_set_cstr(&text, options);

	Lex lex;
	lex_init(&lex);
	lex_start(&lex, &text);

	for (;;)
	{
		// eof
		if (lex_if(&lex, TEOF, NULL))
			break;

		// key [= value]
		Token name;
		if (! lex_if(&lex, TNAME, &name))
			error("config: <name> expected");

		if (str_compare_raw(&name.string, "path", 4))
		{
			// path <string>
			instance_config_string(&lex, &name, &config()->path);
		} else
		if (str_compare_raw(&name.string, "interval", 8))
		{
			// interval <u64>
			instance_config_u64(&lex, &name, &config()->interval);
		} else
		if (str_compare_raw(&name.string, "workers", 7))
		{
			// workers <int>
			instance_config_int(&lex, &name, &config()->workers);
		} else
		if (str_compare_raw(&name.string, "storage", 7))
		{
			// storage name(options)
			instance_config_storage(self, &lex);
		} else {
			error("config: unknown option %.*s", str_size(&name.string),
			      str_of(&name.string));
		}

		// [,]
		if (lex_if(&lex, ',', NULL))
			continue;

		// eof
		if (! lex_if(&lex, TEOF, NULL))
			error("config: unexpected option");

		break;
	}

	// todo
	
	// create default storage
	if (self->storage_mgr.list_count == 0)
	{
		Str name;
		str_set_cstr(&name, "default");
		auto storage = storage_allocate(&name);
		storage_mgr_add(&self->storage_mgr, storage);

		char path[PATH_MAX];
		snprintf(path, sizeof(path), "%s/default", config_path());
		str_strdup(&storage->path, path);
	}
}
