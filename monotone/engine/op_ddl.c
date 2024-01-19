
//
// monotone
//
// time-series storage
//

#include <monotone_runtime.h>
#include <monotone_lib.h>
#include <monotone_io.h>
#include <monotone_catalog.h>
#include <monotone_engine.h>

void
engine_storage_create(Engine* self, Target* target, bool if_not_exists)
{
	// take engine exclusive lock
	engine_lock_global(self, false);
	guard(guard, engine_unlock_global, self);

	// create storage
	storage_mgr_create(&self->storage_mgr, target, if_not_exists);

	// rewrite config file
	config_update();
}

void
engine_storage_drop(Engine* self, Str* name, bool if_exists)
{
	// take engine exclusive lock
	engine_lock_global(self, false);
	guard(guard, engine_unlock_global, self);

	// drop storage
	storage_mgr_drop(&self->storage_mgr, name, if_exists);

	// rewrite config file
	config_update();
}

void
engine_storage_show(Engine* self, Str* name, Buf* buf)
{
	// take engine exclusive lock
	engine_lock_global(self, false);
	guard(guard, engine_unlock_global, self);

	storage_mgr_show(&self->storage_mgr, name, buf);
}

void
engine_storage_show_partitions(Engine* self, Str* name, Buf* buf)
{
	// take engine exclusive lock
	engine_lock_global(self, false);
	guard(guard, engine_unlock_global, self);

	storage_mgr_show_partitions(&self->storage_mgr, name, buf);
}

void
engine_conveyor_alter(Engine* self, List* configs)
{
	// take engine exclusive lock
	engine_lock_global(self, false);
	guard(guard, engine_unlock_global, self);

	// alter conveyor
	conveyor_alter(&self->conveyor, configs);

	// rewrite config file
	config_update();
}

void
engine_conveyor_show(Engine* self, Buf* buf)
{
	// take engine exclusive lock
	engine_lock_global(self, false);
	guard(guard, engine_unlock_global, self);

	conveyor_print(&self->conveyor, buf);
}
