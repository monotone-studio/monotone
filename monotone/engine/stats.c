
//
// monotone
//
// time-series storage
//

#include <monotone_runtime.h>
#include <monotone_lib.h>
#include <monotone_io.h>
#include <monotone_engine.h>

hot Stat*
engine_stats(Engine* self)
{
	mutex_lock(&self->lock);
	guard(unlock, mutex_unlock, &self->lock);

	if (self->list_count == 0)
		return NULL;

	auto stat = (Stat*)nr_malloc(sizeof(Stat) + sizeof(StatPart) * self->tree.tree_count);

	stat->count       = 0;
	stat->count_parts = 0;
	stat->size        = 0;

	auto pos = &stat->parts[0];

	Part* current = part_tree_min(&self->tree);
	while (current)
	{
		pos->min         = current->min;
		pos->max         = current->max;
		pos->cache_count = current->memtable_a.count +
		                   current->memtable_b.count;
		pos->cache_size  = current->memtable_a.size +
		                   current->memtable_b.size;
		pos->count       = pos->cache_count;
		if (current->index)
			pos->count += current->index->count_total;
		pos->size        = pos->cache_size;
		if (current->index)
			pos->size += current->index->size_total;
		pos->tier        = current->storage->order;

		stat->count_parts++;
		stat->count += pos->count;
		stat->size  += pos->size;

		pos++;
		current = part_tree_next(&self->tree, current);
	}

	return stat;
}
