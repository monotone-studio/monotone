
//
// monotone
//
// time-series storage
//

#include <monotone_runtime.h>
#include <monotone_lib.h>
#include <monotone_config.h>
#include <monotone_cloud.h>
#include <monotone_io.h>
#include <monotone_storage.h>
#include <monotone_wal.h>
#include <monotone_engine.h>

static void
engine_rollback(Engine* self, Log* log)
{
	auto pos   = log_last(log);
	auto first = log_first(log);
	while (pos >= first)
	{
		if (pos->ref)
		{
			Ref* ref = pos->ref;
			if (pos->prev)
				memtable_set(ref->part->memtable, pos->prev);
			else
				memtable_unset(ref->part->memtable, pos->event);
			engine_unlock(self, ref, LOCK_ACCESS);
		}
		pos--;
	}
	log_reset(log);
}

hot static void
engine_commit(Engine* self, Log* log)
{
	auto pos = log_first(log);
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
				service_refresh(self->service, part);
				part->refresh = true;
			}

			engine_unlock(self, ref, LOCK_ACCESS);
		}

		pos++;
	}

	log_reset(log);
}

hot void
engine_write(Engine* self, EventArg* events, int count)
{
	Log log;
	log_init(&log);

	Exception e;
	if (try(&e))
	{
		for (int i = 0; i < count; i++)
		{
			auto arg = &events[i];

			// set serial
			if (arg->id == UINT64_MAX && config_serial())
				arg->id = config_ssn_next();

			// prepare log record
			auto op = log_add(&log);

			// find or create partition
			auto ref = engine_lock(self, arg->id, LOCK_ACCESS, false, true);
			op->ref = ref;
			auto memtable = ref->part->memtable;

			// allocate event using current memtables heap
			auto event = event_allocate(&memtable->heap, arg);
			log_add_event(&log, op, event);

			// update memtable and save previous version
			op->prev = memtable_set(memtable, event);
		}

		// wal write
		if (var_int_of(&config()->wal_enable))
		{
			// wal write error test case
			error_injection(error_wal);

			auto rotate_ready = wal_write(self->wal, &log);

			// schedule wal rotation
			if (rotate_ready)
				service_rotate(self->service);
		} else
		{
			// assign next lsn
			log.write.lsn = config_lsn_next();
		}
	}

	if (catch(&e))
	{
		engine_rollback(self, &log);
		log_free(&log);
		rethrow();
	}

	engine_commit(self, &log);
	log_free(&log);
}

void
engine_write_replay(Engine* self, LogWrite* write)
{
	Log log;
	log_init(&log);

	Exception e;
	if (try(&e))
	{
		auto pos = log_write_first(write);
		for (; pos; pos = log_write_next(write, pos))
		{
			auto op = log_add(&log);

			// find or create partition
			auto ref = engine_lock(self, pos->id, LOCK_ACCESS, false, true);
			auto part = ref->part;

			// skip if partition is a newer then write lsn
			if (part->state != ID_NONE && (part->index.lsn >= write->lsn))
			{
				engine_unlock(self, ref, LOCK_ACCESS);
				log_pushback(&log);
				continue;
			}

			// set log reference lock
			op->ref = ref;

			// copy event using current memtables heap
			auto memtable = part->memtable;
			auto event = event_copy(&memtable->heap, pos);
			log_add_event(&log, op, event);

			// update memtable and save previous version
			op->prev = memtable_set(memtable, event);
		}

		// assign lsn
		log.write.lsn = write->lsn;
	}

	if (catch(&e))
	{
		engine_rollback(self, &log);
		log_free(&log);
		rethrow();
	}

	engine_commit(self, &log);
	log_free(&log);

	config_lsn_follow(write->lsn);
}
