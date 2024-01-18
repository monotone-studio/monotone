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
	Row*       current;
	Engine*    engine;
};

hot static inline void
engine_cursor_open(EngineCursor* self, Engine* engine, Row* key)
{
	self->ref     = NULL;
	self->current = NULL;
	self->engine  = engine;

	// find partition
	uint64_t min = 0;
	if (key)
		min = row_interval_min(key);
	self->ref = catalog_lock(&engine->catalog, min, LOCK_ACCESS, true, false);
	if (self->ref == NULL)
		return;

	// open partition cursor
	part_cursor_open(&self->cursor, self->ref->part, key);
	self->current = part_cursor_at(&self->cursor);
}

hot static inline void
engine_cursor_reset(EngineCursor* self)
{
	if (self->ref)
	{
		catalog_unlock(&self->engine->catalog, self->ref, LOCK_ACCESS);
		self->ref = NULL;
	}
	self->current = NULL;
	self->engine  = NULL;
	part_cursor_reset(&self->cursor);
}

hot static inline void
engine_cursor_close(EngineCursor* self)
{
	engine_cursor_reset(self);
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
	if (unlikely(self->ref == NULL))
		return;

	// iterate current partition
	self->current = NULL;
	part_cursor_next(&self->cursor);
	if (likely(part_cursor_has(&self->cursor)))
	{
		self->current = part_cursor_at(&self->cursor);
		return;
	}

	uint64_t next_interval = self->ref->slice.max;

	// close previous partition
	auto catalog = &self->engine->catalog;
	catalog_unlock(catalog, self->ref, LOCK_ACCESS);
	self->ref = NULL;
	part_cursor_reset(&self->cursor);

	// open next partition
	self->ref = catalog_lock(catalog, next_interval, LOCK_ACCESS, true, false);
	if (unlikely(self->ref == NULL))
		return;

	part_cursor_open(&self->cursor, self->ref->part, NULL);
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
	self->ref     = NULL;
	self->current = NULL;
	self->engine  = NULL;
	part_cursor_init(&self->cursor);
}
