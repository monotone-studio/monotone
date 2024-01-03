
//
// monotone
//
// time-series storage
//

#include <monotone_runtime.h>
#include <monotone_lib.h>
#include <monotone_io.h>
#include <monotone_engine.h>

static void
engine_recover_storage(Engine* self, Storage* storage)
{
	if (storage->capacity == 0)
		return;

	// create storage directory, if not exists
	const char* path = str_of(&storage->path);
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
		if (str_empty(&storage->path))
			continue;
		engine_recover_storage(self, storage);
	}
}
