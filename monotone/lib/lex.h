#pragma once

//
// monotone
//
// time-series storage
//

typedef struct Token Token;
typedef struct Lex   Lex;

enum
{
	KEOF,
	KREAL,
	KINT,
	KSTRING,
	KNAME,
	// keywords
	KTRUE,
	KFALSE,
	KSET,
	KTO,
	KSHOW,
	KCREATE
};

struct Token
{
	int id;
	union {
		uint64_t integer;
		double   real;
		struct {
			Str  string;
			bool string_escape;
		};
	};
};

struct Lex
{
	char* pos;
	char* end;
	Token backlog[4];
	int   backlog_size;
};

static inline void
token_init(Token* tk)
{
	memset(tk, 0, sizeof(*tk));
}

void lex_init(Lex*);
void lex_start(Lex*, Str*);
void lex_next(Lex*, Token*);
void lex_push(Lex*, Token*);
bool lex_if(Lex*, int, Token*);
