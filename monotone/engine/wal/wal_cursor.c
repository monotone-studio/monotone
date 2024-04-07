
//
// monotone.
//
// embeddable cloud-native storage for events
// and time-series data.
//
// Copyright (c) 2023-2024 Dmitry Simonenko
// Copyright (c) 2023-2024 Monotone
//
// Distributed under the terms of GNU LGPL v3 or
// Monotone Commercial License.
//

#include <monotone_runtime.h>
#include <monotone_lib.h>
#include <monotone_config.h>
#include <monotone_cloud.h>
#include <monotone_io.h>
#include <monotone_storage.h>
#include <monotone_wal.h>

void
wal_cursor_init(WalCursor* self)
{
	self->file_offset = 0;
	self->file        = NULL;
	self->wal         = NULL;
	buf_init(&self->buf);
}

void
wal_cursor_open(WalCursor* self, Wal* wal, uint64_t lsn)
{
	self->file        = NULL;
	self->file_offset = 0;
	self->wal         = wal;

	// find nearest file with id <= lsn
	uint64_t id;
	if (lsn == 0)
		id = wal_id_min(&wal->list);
	else
		id = wal_id_find(&wal->list, lsn);
	if (id == UINT64_MAX)
		return;
	self->file = wal_file_allocate(id);
	wal_file_open(self->file);

	// rewind to the start lsn
	for (;;)
	{
		if (! wal_cursor_next(self))
			break;
		auto write = wal_cursor_at(self);
		if (write->lsn >= lsn)
		{
			// rewind
			self->file_offset -= write->size;
			break;
		}
	}
}

void
wal_cursor_close(WalCursor* self)
{
	if (self->file)
	{
		wal_file_close(self->file);
		wal_file_free(self->file);
		self->file = NULL;
	}
	buf_free(&self->buf);
}

LogWrite*
wal_cursor_at(WalCursor* self)
{
	return (LogWrite*)self->buf.start;
}

bool
wal_cursor_next(WalCursor* self)
{
	if (unlikely(self->file == NULL))
		return false;

	auto wal = self->wal;
	for (;;)
	{
		auto file = self->file;
		if (wal_file_pread(file, self->file_offset, &self->buf))
		{
			auto write = wal_cursor_at(self);
			self->file_offset += write->size;
			return true;
		}

		// get to the next file id
		uint64_t id;
		id = wal_id_next(&wal->list, file->id);
		if (id == UINT64_MAX)
			break;

		// close previous file
		wal_file_close(file);
		wal_file_free(file);

		// open next file
		self->file_offset = 0;
		self->file        = NULL;
		self->file        = wal_file_allocate(id);
		wal_file_open(self->file);
	}

	return false;
}
