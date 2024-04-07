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

typedef struct LogWrite LogWrite;

enum
{
	LOG_WRITE,
	LOG_DROP
};

struct LogWrite
{
	uint32_t crc;
	uint64_t lsn;
	uint8_t  type;
	uint32_t count;
	uint32_t size;
} packed;

static inline void
log_write_init(LogWrite* self)
{
	self->crc   = 0;
	self->lsn   = 0;
	self->type  = LOG_WRITE;
	self->count = 0;
	self->size  = 0;
}

static inline void
log_write_reset(LogWrite* self)
{
	self->crc   = 0;
	self->lsn   = 0;
	self->type  = LOG_WRITE;
	self->count = 0;
	self->size  = 0;
}

static inline Event*
log_write_first(LogWrite* self)
{
	return (Event*)((uintptr_t)self + sizeof(*self));
}

static inline Event*
log_write_next(LogWrite* self, Event* at)
{
	auto next = (uintptr_t)at + event_size(at);
	auto eof  = (uintptr_t)self + self->size;
	if (next >= eof)
		return NULL;
	return (Event*)next;
}

static inline void
log_write_seal(LogWrite* self)
{
	if (var_int_of(&config()->wal_crc))
		self->crc = crc32(self->crc, (char*)self + sizeof(uint32_t),
		                  sizeof(LogWrite) - sizeof(uint32_t));
}
