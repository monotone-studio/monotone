#pragma once

//
// monotone
//
// time-series storage
//

typedef struct LogDrop LogDrop;

struct LogDrop
{
	LogWrite write;
	uint64_t id;
} packed;

static inline void
log_drop_init(LogDrop* self)
{
	auto write = &self->write;
	write->lsn   = 0;
	write->type  = LOG_DROP;
	write->count = 0;
	write->size  = sizeof(LogDrop);
	self->id     = 0;
}
