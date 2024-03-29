#pragma once

//
// monotone
//
// time-series storage
//

typedef struct EngineCursor EngineCursor;

struct EngineCursor
{
	Ref*       ref;
	PartCursor cursor;
	Engine*    engine;
};

hot static inline void
engine_cursor_open_part(EngineCursor* self, uint64_t min, Event* key)
{
	auto engine = self->engine;

	// match and lock partition
	self->ref = engine_lock(engine, min, LOCK_ACCESS, true, false);
	if (self->ref == NULL)
		return;

	// open partition cursor
	part_cursor_open(&self->cursor, self->ref->part, key);
}

hot static inline void
engine_cursor_next(EngineCursor*);

hot static inline void
engine_cursor_open(EngineCursor* self, Engine* engine, Event* key)
{
	self->ref    = NULL;
	self->engine = engine;

	// find partition
	uint64_t min = 0;
	if (key)
		min = key->id;
	engine_cursor_open_part(self, min, key);

	// partition found but cursor key set to > max
	if (self->ref && !part_cursor_has(&self->cursor))
		engine_cursor_next(self);
}

hot static inline void
engine_cursor_reset(EngineCursor* self)
{
	part_cursor_close(&self->cursor);
	if (self->ref)
	{
		engine_unlock(self->engine, self->ref, LOCK_ACCESS);
		self->ref = NULL;
	}
	self->engine = NULL;
}

hot static inline void
engine_cursor_close(EngineCursor* self)
{
	engine_cursor_reset(self);
	part_cursor_free(&self->cursor);
}

hot static inline Event*
engine_cursor_at(EngineCursor* self)
{
	return part_cursor_at(&self->cursor);
}

hot static inline bool
engine_cursor_has(EngineCursor* self)
{
	return self->ref != NULL;
}

hot static inline void
engine_cursor_next(EngineCursor* self)
{
	if (unlikely(self->ref == NULL))
		return;

	// iterate current partition
	part_cursor_next(&self->cursor);
	if (likely(part_cursor_has(&self->cursor)))
		return;

	uint64_t next_interval = self->ref->slice.max + 1;

	// close previous partition
	part_cursor_close(&self->cursor);
	engine_unlock(self->engine, self->ref, LOCK_ACCESS);
	self->ref = NULL;

	// open next partition
	engine_cursor_open_part(self, next_interval, NULL);
}

hot static inline void
engine_cursor_skip_deletes(EngineCursor* self)
{
	// skip deletes and empty partitions
	while (engine_cursor_has(self))
	{
		auto at = engine_cursor_at(self);
		if (at && !at->is_delete)
			break;
		engine_cursor_next(self);
	}
}

hot static inline void
engine_cursor_init(EngineCursor* self)
{
	self->ref    = NULL;
	self->engine = NULL;
	part_cursor_init(&self->cursor);
}
