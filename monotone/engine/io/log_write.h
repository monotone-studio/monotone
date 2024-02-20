#pragma once

//
// monotone
//
// time-series storage
//

typedef struct LogWrite LogWrite;

enum
{
	LOG_WRITE
};

struct LogWrite
{
	uint64_t lsn;
	uint8_t  type;
	uint32_t count;
	uint32_t size;
} packed;

static inline void
log_write_init(LogWrite* self)
{
	self->lsn   = 0;
	self->type  = LOG_WRITE;
	self->count = 0;
	self->size  = 0;
}

static inline void
log_write_reset(LogWrite* self)
{
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
