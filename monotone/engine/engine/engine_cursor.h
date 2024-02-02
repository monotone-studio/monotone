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
engine_cursor_open_part(EngineCursor* self, uint64_t min, Row* key)
{
	auto engine = self->engine;

	// match and lock partition
	self->ref = engine_lock(engine, min, LOCK_ACCESS, true, false);
	if (self->ref == NULL)
		return;
	auto part = self->ref->part;

	// match storage and get cloud
	Cloud* cloud = NULL;
	if (! str_empty(&part->source->cloud))
	{
		auto storage = storage_mgr_find(&engine->storage_mgr, &part->source->name);
		assert(storage);
		cloud = storage->cloud;
	}

	// open partition cursor
	part_cursor_open(&self->cursor, cloud, part, key);
	self->current = part_cursor_at(&self->cursor);
}

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

	engine_cursor_open_part(self, min, key);
}

hot static inline void
engine_cursor_reset(EngineCursor* self)
{
	if (self->ref)
	{
		engine_unlock(self->engine, self->ref, LOCK_ACCESS);
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
	engine_unlock(self->engine, self->ref, LOCK_ACCESS);
	self->ref = NULL;
	part_cursor_reset(&self->cursor);

	// open next partition
	engine_cursor_open_part(self, next_interval, NULL);
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
