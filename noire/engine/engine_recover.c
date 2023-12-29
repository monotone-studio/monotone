
//
// noire
//
// time-series storage
//

#include <noire_runtime.h>
#include <noire_lib.h>
#include <noire_io.h>
#include <noire_engine.h>

static void
engine_recover_storage(Engine* self, Storage* storage)
{
	// create storage directory, if not exists
	const char* path = str_of(&storage->directory);
	if (! fs_exists("%s", path))
	{
		log("storage: new directory '%s'", path);
		fs_mkdir(0755, "%s", path);
		return;
	}

	(void)self;

	// read directory
}

void
engine_recover(Engine* self)
{
	// read partitions per storage
	list_foreach(&self->tier_mgr.storage_mgr->list)
	{
		auto storage = list_at(Storage, link);
		if (str_empty(&storage->directory))
			continue;
		engine_recover_storage(self, storage);
	}
}
