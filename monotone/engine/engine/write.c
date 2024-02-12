
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
#include <monotone_wal.h>
#include <monotone_engine.h>

hot void
engine_insert(Engine* self, Log* log, uint64_t time,
              void*   data,
              int     data_size)
{
	unused(self);

	auto op  = log_add(log);
	auto row = row_allocate(time, data, data_size);
	log_set(log, op, row);

	// update stats
	var_int_add(&config()->rows_written, 1);
	var_int_add(&config()->rows_written_bytes, row_size(op->row));
}

hot void
engine_delete(Engine* self, Log* log, uint64_t time,
              void*   data,
              int     data_size)
{
	unused(self);

	auto op  = log_add(log);
	auto row = row_allocate(time, data, data_size);
	row->is_delete = true;
	log_set(log, op, row);

	// update stats
	var_int_add(&config()->rows_written, 1);
	var_int_add(&config()->rows_written_bytes, row_size(op->row));
}

static void
engine_rollback(Engine* self, Log* log)
{
	(void)self;
	// todo: free rows, set prev rows back
	//
	//engine_unlock(self, ref, LOCK_ACCESS);
	log_reset(log);
}

static void
engine_commit(Engine* self, Log* log)
{
	auto pos = log_begin(log);
	auto end = log_end(log);
	while (pos < end)
	{
		if (pos->ref)
		{
			Ref* ref  = pos->ref;
			auto part = ref->part;
			memtable_follow(ref->part->memtable, log->write.lsn);

			// schedule refresh
			if (part_refresh_ready(part))
			{
				service_refresh(self->service, part->id.min);
				part->refresh = true;
			}

			engine_unlock(self, ref, LOCK_ACCESS);
		}

		if (pos->prev)
			row_free(pos->prev);

		pos++;
	}

	log_reset(log);
}

hot void
engine_write(Engine* self, Log* log)
{
	Exception e;
	if (try(&e))
	{
		auto pos = log_begin(log);
		auto end = log_end(log);
		while (pos < end)
		{
			auto row = pos->row;

			// get partition min interval
			uint64_t min = row_interval_min(row);

			// find or create partition
			auto ref = engine_lock(self, min, LOCK_ACCESS, false, true);
			pos->ref = ref;

			// update memtable and save previous version
			pos->prev = memtable_set(ref->part->memtable, row);
			pos++;
		}

		// wal write
		auto rotate_ready = wal_write(self->wal, log);
		if (rotate_ready)
		{
			// todo: service
		}
	}

	if (catch(&e))
	{
		engine_rollback(self, log);
		rethrow();
	}

	engine_commit(self, log);
}
