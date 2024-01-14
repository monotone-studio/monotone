
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

hot static inline void
engine_write_to(Engine* self, Lock* lock, Row* row)
{
	Part* part = lock->part;

	// update stats
	atomic_u64_add(&self->rows_written, 1);
	atomic_u64_add(&self->rows_written_bytes, row_size(row));

	// update memtable
	auto memtable = part->memtable;
	memtable_set(memtable, row);

	// todo: wal write
	memtable_follow(memtable, 0);

	// schedule compaction
	if (part->target->compaction_wm > 0)
	{
		if (part->memtable->size > (uint32_t)part->target->compaction_wm)
			service_add_if_not_pending(&self->service, SERVICE_MERGE, part->min, NULL);
	}
}

hot void
engine_write(Engine* self, bool delete, uint64_t time,
             void*   data,
             int     data_size)
{
	// insert, replace or delete by key

	// allocate row
	auto row = row_allocate(time, data, data_size);
	if (delete)
		row->is_delete = true;

	// get partition min interval
	uint64_t min = row_interval_min(row);

	// find or create partition
	auto lock = engine_find(self, true, min);
	guard(unlock, lock_mgr_unlock, lock);

	// write
	engine_write_to(self, lock, row);
}

void
engine_write_by(Engine*       self,
                EngineCursor* cursor,
                bool          delete,
                uint64_t      time,
                void*         data,
                int           data_size)
{
	// update or delete by cursor
	if (unlikely(! engine_cursor_has(cursor)))
		error("cursor is not active");

	Row* current = cursor->current;
	Row* row     = NULL;
	if (delete)
	{
		row = row_allocate(current->time, current->data, current->data_size);
		row->is_delete = true;
	} else
	{
		row = row_allocate(time, data, data_size);
		if (unlikely(compare(self->comparator, row, current) != 0))
		{
			row_free(row);
			error("key does not match cursor row");
		}
	}

	// use cursor partition lock
	engine_write_to(self, cursor->lock, row);

	// update cursor
	if (delete)
		cursor->current = NULL;
	else
		cursor->current = row;
}
