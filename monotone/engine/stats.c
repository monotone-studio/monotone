
//
// monotone
//
// time-series storage
//

#include <monotone_runtime.h>
#include <monotone_lib.h>
#include <monotone_io.h>
#include <monotone_engine.h>

StatStorage*
engine_stats(Engine* self, Stat* stat)
{
	mutex_lock(&self->lock);
	guard(unlock, mutex_unlock, &self->lock);

	// stat per storage
	int  size = sizeof(StatStorage) * self->tier_mgr.storage_mgr->list_count;
	auto storages = (StatStorage*)mn_malloc(size);
	memset(storages, 0, size);

	list_foreach(&self->tier_mgr.storage_mgr->list)
	{
		auto storage = list_at(Storage, link);
		auto ref = &storages[storage->order];
		if (storage->capacity > 0)
			ref->min  = UINT64_MAX;
		ref->max  = 0;
		ref->name = str_of(&storage->name);
	}

	auto current = part_tree_min(&self->tree);
	while (current)
	{
		auto storage = current->storage;
		auto ref = &storages[storage->order];

		// partitions
		ref->partitions++;

		if (current->service)
			ref->pending++;

		if (current->min < ref->min)
			ref->min = current->min;
		if (current->max > ref->max)
			ref->max = current->max;

		// rows
		if (current->index)
			ref->rows += current->index->count_total;
		uint64_t rows_cached = current->memtable_a.count + current->memtable_b.count;
		ref->rows_cached += rows_cached;
		ref->rows += rows_cached;

		// size
		if (current->index)
		{
			ref->size += current->index->size_total;
			ref->size_uncompressed += current->index->size_total_origin;
		}
		uint64_t size_cached = current->memtable_a.size + current->memtable_b.size;
		ref->size_cached += size_cached;
		ref->size += size_cached;

		current = part_tree_next(&self->tree, current);
	}

	// stat
	memset(stat, 0, sizeof(*stat));
	stat->storages           = self->tier_mgr.storage_mgr->list_count;
	stat->lsn                = 0;
	stat->rows_written       = atomic_u64_of(&self->rows_written);
	stat->rows_written_bytes = atomic_u64_of(&self->rows_written_bytes);
	stat->min                = UINT64_MAX;
	stat->max                = 0;

	list_foreach(&self->tier_mgr.storage_mgr->list)
	{
		auto storage = list_at(Storage, link);
		auto ref = &storages[storage->order];

		// partitions
		stat->partitions += ref->partitions;
		stat->pending    += ref->pending;
		if (ref->min < stat->min)
			stat->min = ref->min;
		if (ref->max > stat->max)
			stat->max = ref->max;

		// rows
		stat->rows += ref->rows;
		stat->rows_cached += ref->rows_cached;

		// size
		stat->size += ref->size;
		stat->size_uncompressed += ref->size_uncompressed;
		stat->size_cached += ref->size_cached;
	}

	return storages;
}
