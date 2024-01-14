#pragma once

//
// monotone
//
// time-series storage
//

typedef struct EngineCursor EngineCursor;

struct EngineCursor
{
	Lock*      lock;
	PartCursor cursor;
	Row*       current;
	Engine*    engine;
};

hot static inline void
engine_cursor_open(EngineCursor* self, Engine* engine, Row* key)
{
	self->lock    = NULL;
	self->current = NULL;
	self->engine  = engine;

	// find partition
	uint64_t min = 0;
	if (key)
		min = row_interval_min(key);
	self->lock = engine_seek(engine, min);
	if (self->lock == NULL)
		return;

	// open partition cursor
	auto part = self->lock->part;
	part_cursor_open(&self->cursor, part, key);
	self->current = part_cursor_at(&self->cursor);
}

hot static inline void
engine_cursor_close(EngineCursor* self)
{
	if (self->lock)
	{
		lock_mgr_unlock(self->lock);
		self->lock = NULL;
	}
	self->current = NULL;
	self->engine  = NULL;
	part_cursor_free(&self->cursor);
}

hot static inline Row*
engine_cursor_at(EngineCursor* self)
{
	return self->current;
}

hot static inline bool
engine_cursor_has(EngineCursor* self)
{
	return self->current != NULL;
}

hot static inline void
engine_cursor_next(EngineCursor* self)
{
	if (unlikely(self->lock == NULL))
		return;

	// iterate current partition
	self->current = NULL;
	part_cursor_next(&self->cursor);
	if (likely(part_cursor_has(&self->cursor)))
	{
		self->current = part_cursor_at(&self->cursor);
		return;
	}

	Part* part = self->lock->part;
	uint64_t next_interval = part->max;

	// close previous partition
	lock_mgr_unlock(self->lock);
	self->lock = NULL;
	part_cursor_reset(&self->cursor);

	// open next partition
	self->lock = engine_seek(self->engine, next_interval);
	if (unlikely(self->lock == NULL))
		return;

	part = self->lock->part;
	part_cursor_open(&self->cursor, part, NULL);
	self->current = part_cursor_at(&self->cursor);
}

hot static inline void
engine_cursor_skip_deletes(EngineCursor* self)
{
	for (;;)
	{
		auto at = engine_cursor_at(self);
		if (!at || !at->is_delete)
			break;
		engine_cursor_next(self);
	}
}

hot static inline void
engine_cursor_init(EngineCursor* self)
{
	self->lock    = NULL;
	self->current = NULL;
	self->engine  = NULL;
	part_cursor_init(&self->cursor);
}

hot static inline void
engine_cursor_reset(EngineCursor* self)
{
	if (self->lock)
	{
		lock_mgr_unlock(self->lock);
		self->lock = NULL;
	}
	self->current = NULL;
	self->engine  = NULL;
	part_cursor_reset(&self->cursor);
}
