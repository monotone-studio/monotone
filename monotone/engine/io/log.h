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
	Row*  row;
	Row*  prev;
	void* ref;
};

struct Log
{
	Buf op; 
	int op_count;
};

static inline LogOp*
log_of(Log* self, int pos)
{
	auto op = (LogOp*)self->op.start;
	return &op[pos];
}

static inline void
log_init(Log* self)
{
	self->op_count = 0;
	buf_init(&self->op);
}

static inline void
log_free(Log* self)
{
	buf_free(&self->op);
}

static inline void
log_cleanup(Log* self)
{
	for (int pos = 0; pos < self->op_count; pos++)
	{
		auto op = log_of(self, pos);
		if (op->row)
			row_free(op->row);
	}
}

static inline void
log_reset(Log* self)
{
	self->op_count = 0;
	buf_reset(&self->op);
}

hot static inline LogOp*
log_add(Log* self)
{
	buf_reserve(&self->op, sizeof(LogOp));
	auto op = (LogOp*)self->op.position;
	op->row  = NULL;
	op->prev = NULL;
	op->ref  = NULL;
	buf_advance(&self->op, sizeof(LogOp));
	self->op_count++;
	return op;
}
