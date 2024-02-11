
//
// monotone
//
// time-series storage
//

#include <monotone_runtime.h>
#include <monotone_lib.h>
#include <monotone_config.h>
#include <monotone_io.h>
#include <monotone_storage.h>
#include <monotone_engine.h>

hot void
engine_insert(Engine* self, Log* log, uint64_t time,
              void*   data,
              int     data_size)
{
	unused(self);
	auto op = log_add(log);
	op->row = row_allocate(time, data, data_size);
}

hot void
engine_delete(Engine* self, Log* log, uint64_t time,
              void*   data,
              int     data_size)
{
	unused(self);
	auto op = log_add(log);
	op->row = row_allocate(time, data, data_size);
	op->row->is_delete = true;
}

hot static inline void
engine_write_to(Engine* self, Ref* ref, Row* row)
{
	Part* part = ref->part;

	// update stats
	var_int_add(&config()->rows_written, 1);
	var_int_add(&config()->rows_written_bytes, row_size(row));

	// update memtable
	auto memtable = part->memtable;
	memtable_set(memtable, row);

	// todo: wal write
	memtable_follow(memtable, 0);

	// schedule refresh
	if (part_refresh_ready(part))
	{
		service_refresh(self->service, part->id.min);
		part->refresh = true;
	}
}

hot void
engine_write(Engine* self, Log* log)
{
	// todo: rollback

	for (int pos = 0; pos < log->op_count; pos++)
	{
		auto op = log_of(log, pos);
		auto row = op->row;

		// get partition min interval
		uint64_t min = row_interval_min(row);

		// find or create partition
		auto ref = engine_lock(self, min, LOCK_ACCESS, false, true);

		// write
		Exception e;
		if (try(&e))
		{
			engine_write_to(self, ref, row);
		}
		engine_unlock(self, ref, LOCK_ACCESS);

		if (catch(&e))
			rethrow();
	}

	log_reset(log);
}
