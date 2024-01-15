
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
engine_storage_show(Engine* self, Str* name, Buf* buf)
{
	// take engine exclusive lock
	lock_mgr_lock_exclusive(&self->lock_mgr);
	guard(lock_guard, lock_mgr_unlock_exclusive, &self->lock_mgr);

	storage_mgr_show(&self->storage_mgr, name, buf);
}

void
engine_storage_show_all(Engine* self, Buf* buf)
{
	// take engine exclusive lock
	lock_mgr_lock_exclusive(&self->lock_mgr);
	guard(lock_guard, lock_mgr_unlock_exclusive, &self->lock_mgr);

	storage_mgr_show_all(&self->storage_mgr, buf);
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

void
engine_checkpoint(Engine* self)
{
	// take engine exclusive lock
	lock_mgr_lock_exclusive(&self->lock_mgr);
	guard(lock_guard, lock_mgr_unlock_exclusive, &self->lock_mgr);

	auto part = part_tree_min(&self->tree);
	while (part)
	{
		if (part->memtable->size > 0)
			service_add_if_not_pending(&self->service, SERVICE_MERGE, part->min, NULL);
		part = part_tree_next(&self->tree, part);
	}
}

void
engine_partition_move(Engine* self, uint64_t min, Str* name, bool if_exists)
{
	// take engine exclusive lock
	lock_mgr_lock_exclusive(&self->lock_mgr);
	guard(lock_guard, lock_mgr_unlock_exclusive, &self->lock_mgr);

	auto part = part_tree_match(&self->tree, min);
	if (! part)
	{
		if (! if_exists)
			error("partition: %" PRIu64 " is not found", min);
		return;
	}

	auto storage = storage_mgr_find(&self->storage_mgr, name);
	if (! storage)
		error("storage '%.*s': not exists", str_size(name),
		      str_of(name));

	service_add(&self->service, SERVICE_MOVE, min, name);
}

void
engine_partition_drop(Engine* self, uint64_t min, bool if_exists)
{
	// take engine exclusive lock
	lock_mgr_lock_exclusive(&self->lock_mgr);
	guard(lock_guard, lock_mgr_unlock_exclusive, &self->lock_mgr);

	auto part = part_tree_match(&self->tree, min);
	if (! part)
	{
		if (! if_exists)
			error("partition: %" PRIu64 " is not found", min);
		return;
	}

	service_add(&self->service, SERVICE_DROP, min, NULL);
}

void
engine_partitions_move(Engine* self, uint64_t min, uint64_t max, Str* name)
{
	// take engine exclusive lock
	lock_mgr_lock_exclusive(&self->lock_mgr);
	guard(lock_guard, lock_mgr_unlock_exclusive, &self->lock_mgr);

	auto storage = storage_mgr_find(&self->storage_mgr, name);
	if (! storage)
		error("storage '%.*s': not exists", str_size(name),
		      str_of(name));

	if (self->tree.tree_count == 0)
		return;

	auto part = part_tree_seek(&self->tree, min);
	while (part)
	{
		if (part->min >= max)
			return;
		service_add(&self->service, SERVICE_MOVE, part->min, name);
		part = part_tree_next(&self->tree, part);
	}
}

void
engine_partitions_drop(Engine* self, uint64_t min, uint64_t max)
{
	// take engine exclusive lock
	lock_mgr_lock_exclusive(&self->lock_mgr);
	guard(lock_guard, lock_mgr_unlock_exclusive, &self->lock_mgr);

	if (self->tree.tree_count == 0)
		return;

	auto part = part_tree_seek(&self->tree, min);
	while (part)
	{
		if (part->min >= max)
			return;
		service_add(&self->service, SERVICE_DROP, part->min, NULL);
		part = part_tree_next(&self->tree, part);
	}
}
