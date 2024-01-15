
//
// monotone
//
// time-series storage
//

#include <monotone_runtime.h>
#include <monotone_lib.h>
#include <monotone_io.h>
#include <monotone_storage.h>
#include <monotone_engine.h>

void
engine_storage_create(Engine* self, Target* target, bool if_not_exists)
{
	// take engine exclusive lock
	lock_mgr_lock_exclusive(&self->lock_mgr);
	guard(lock_guard, lock_mgr_unlock_exclusive, &self->lock_mgr);

	// create storage
	storage_mgr_create(&self->storage_mgr, target, if_not_exists);

	// rewrite config file
	config_update();
}

void
engine_storage_drop(Engine* self, Str* name, bool if_exists)
{
	// take engine exclusive lock
	lock_mgr_lock_exclusive(&self->lock_mgr);
	guard(lock_guard, lock_mgr_unlock_exclusive, &self->lock_mgr);

	// drop storage
	storage_mgr_drop(&self->storage_mgr, name, if_exists);

	// rewrite config file
	config_update();
}

void
engine_storage_show(Engine* self, Buf* buf)
{
	// take engine exclusive lock
	lock_mgr_lock_exclusive(&self->lock_mgr);
	guard(lock_guard, lock_mgr_unlock_exclusive, &self->lock_mgr);

	(void)self;
	(void)buf;
}

void
engine_conveyor_alter(Engine* self, List* configs)
{
	// take engine exclusive lock
	lock_mgr_lock_exclusive(&self->lock_mgr);
	guard(lock_guard, lock_mgr_unlock_exclusive, &self->lock_mgr);

	// alter conveyor
	conveyor_alter(&self->conveyor, configs);

	// rewrite config file
	config_update();
}

void
engine_conveyor_show(Engine* self, Buf* buf)
{
	// take engine exclusive lock
	lock_mgr_lock_exclusive(&self->lock_mgr);
	guard(lock_guard, lock_mgr_unlock_exclusive, &self->lock_mgr);

	conveyor_print(&self->conveyor, buf);
}
