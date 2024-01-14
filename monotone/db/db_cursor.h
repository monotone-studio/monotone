#pragma once

//
// monotone
//
// time-series storage
//

typedef struct DbCursor DbCursor;

struct DbCursor
{
	Lock*      lock;
	PartCursor cursor;
	Row*       current;
	Db*        db;
};

hot static inline void
db_cursor_open(DbCursor* self, Db* db, Row* key)
{
	self->lock    = NULL;
	self->current = NULL;
	self->db      = db;

	// find partition
	uint64_t min = 0;
	if (key)
		min = row_interval_min(key);
	self->lock = db_seek(db, min);
	if (self->lock == NULL)
		return;

	// open partition cursor
	auto part = self->lock->part;
	part_cursor_open(&self->cursor, part, key);
	self->current = part_cursor_at(&self->cursor);
}

hot static inline void
db_cursor_close(DbCursor* self)
{
	if (self->lock)
	{
		lock_mgr_unlock(self->lock);
		self->lock = NULL;
	}
	self->current = NULL;
	self->db = NULL;
	part_cursor_free(&self->cursor);
}

hot static inline Row*
db_cursor_at(DbCursor* self)
{
	return self->current;
}

hot static inline bool
db_cursor_has(DbCursor* self)
{
	return self->current != NULL;
}

hot static inline void
db_cursor_next(DbCursor* self)
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
	self->lock = db_seek(self->db, next_interval);
	if (unlikely(self->lock == NULL))
		return;

	part = self->lock->part;
	part_cursor_open(&self->cursor, part, NULL);
	self->current = part_cursor_at(&self->cursor);
}

hot static inline void
db_cursor_skip_deletes(DbCursor* self)
{
	for (;;)
	{
		auto at = db_cursor_at(self);
		if (!at || !at->is_delete)
			break;
		db_cursor_next(self);
	}
}

hot static inline void
db_cursor_init(DbCursor* self)
{
	self->lock    = NULL;
	self->current = NULL;
	self->db  = NULL;
	part_cursor_init(&self->cursor);
}

hot static inline void
db_cursor_reset(DbCursor* self)
{
	if (self->lock)
	{
		lock_mgr_unlock(self->lock);
		self->lock = NULL;
	}
	self->current = NULL;
	self->db  = NULL;
	part_cursor_reset(&self->cursor);
}
