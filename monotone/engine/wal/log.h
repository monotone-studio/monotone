#pragma once

//
// monotone
//
// time-series storage
//

typedef struct LogOp LogOp;
typedef struct Log   Log;

struct LogOp
{
	Event* event;
	Event* prev;
	void*  ref;
};

struct Log
{
	Buf      op;
	LogWrite write;
	Iov      iov;
};

static inline void
log_init(Log* self)
{
	buf_init(&self->op);
	log_write_init(&self->write);
	iov_init(&self->iov);
}

static inline void
log_free(Log* self)
{
	buf_free(&self->op);
	iov_free(&self->iov);
}

static inline LogOp*
log_of(Log* self, int pos)
{
	auto op = (LogOp*)self->op.start;
	return &op[pos];
}

static inline LogOp*
log_first(Log* self)
{
	return log_of(self, 0);
}

static inline LogOp*
log_last(Log* self)
{
	return log_of(self, self->write.count - 1);
}

static inline LogOp*
log_end(Log* self)
{
	return log_of(self, self->write.count);
}

static inline void
log_cleanup(Log* self)
{
	auto pos = log_first(self);
	auto end = log_end(self);
	for (; pos < end; pos++)
	{
		if (pos->event)
			event_free(pos->event);
	}
}

static inline void
log_reset(Log* self)
{
	buf_reset(&self->op);
	log_write_reset(&self->write);
	iov_reset(&self->iov);
}

hot static inline LogOp*
log_add(Log* self)
{
	buf_reserve(&self->op, sizeof(LogOp));
	auto op = (LogOp*)self->op.position;
	op->event = NULL;
	op->prev  = NULL;
	op->ref   = NULL;
	self->write.count++;
	buf_advance(&self->op, sizeof(LogOp));
	return op;
}

hot static inline void
log_add_event(Log* self, LogOp* op, Event* event)
{
	// add log write header before the first entry
	if (self->write.count == 1)
	{
		iov_add(&self->iov, &self->write, sizeof(self->write));
		self->write.size += sizeof(LogWrite);
	}
	iov_add(&self->iov, event, event_size(event));
	op->event = event;
	self->write.size += event_size(event);
}

hot static inline void
log_pushback(Log* self)
{
	self->write.count--;
	buf_truncate(&self->op, sizeof(LogOp));
}
