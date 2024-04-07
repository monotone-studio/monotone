#pragma once

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

typedef struct WalFile WalFile;

struct WalFile
{
	uint64_t id;
	File     file;
};

static inline WalFile*
wal_file_allocate(uint64_t id)
{
	WalFile* self = mn_malloc(sizeof(WalFile));
	self->id = id;
	file_init(&self->file);
	return self;
}

static inline void
wal_file_free(WalFile* self)
{
	mn_free(self);
}

static inline void
wal_file_open(WalFile* self)
{
	char path[PATH_MAX];
	snprintf(path, sizeof(path), "%s/wal/%020" PRIu64 ".wal",
	         config_directory(),
	         self->id);
	file_open(&self->file, path);
}

static inline void
wal_file_create(WalFile* self)
{
	char path[PATH_MAX];
	snprintf(path, sizeof(path), "%s/wal/%020" PRIu64 ".wal",
	         config_directory(),
	         self->id);
	file_create(&self->file, path);
}

static inline void
wal_file_close(WalFile* self)
{
	file_close(&self->file);
}

static inline void
wal_file_write(WalFile* self, void* data, int data_size)
{
	file_write(&self->file, data, data_size);
	if (var_int_of(&config()->wal_sync_on_write))
		file_sync(&self->file);
}

static inline void
wal_file_writev(WalFile* self, struct iovec* iov, int iovc)
{
	file_writev(&self->file, iov, iovc);
	if (var_int_of(&config()->wal_sync_on_write))
		file_sync(&self->file);
}

static inline bool
wal_file_eof(WalFile* self, uint32_t offset, uint32_t size)
{
	return (offset + size) > self->file.size;
}

static inline bool
wal_file_pread(WalFile* self, uint64_t offset, Buf* buf)
{
	buf_reset(buf);

	// check for eof
	if (wal_file_eof(self, offset, sizeof(LogWrite)))
		return false;

	// read header
	uint32_t size_header = sizeof(LogWrite);
	int start = buf_size(buf);
	file_pread_buf(&self->file, buf, size_header, offset);
	uint32_t size = ((LogWrite*)(buf->start + start))->size;

	// check for eof
	if (wal_file_eof(self, offset, size))
		return false;

	// read body
	uint32_t size_data;
	size_data = size - size_header;
	file_pread_buf(&self->file, buf, size_data, offset + size_header);

	// check crc
	if (var_int_of(&config()->wal_crc))
	{
		auto write = (LogWrite*)(buf->start + start);
		uint32_t crc = 0;
		crc = crc32(crc, buf->start + start + size_header, size_data);
		crc = crc32(crc, (char*)write + sizeof(uint32_t), size_header - sizeof(uint32_t));
		if (crc != write->crc)
			error("wal: file '%s' crc mismatch at offset %" PRIu64 " (lsn %" PRIu64 ")",
			      str_of(&self->file.path),
			      offset, write->lsn);
	}

	return true;
}
