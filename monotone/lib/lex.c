
//
// monotone
//
// time-series storage
//

#include <monotone_runtime.h>
#include <monotone_lib.h>

typedef struct
{
	int         id;
	const char* name;
	int         name_size;
} Keyword;

static Keyword keywords[] =
{
	{ KTRUE,   "true",     4},
	{ KFALSE,  "false",    5},
	{ KSET,    "set",      3},
	{ KTO,     "to",       2},
	{ KSHOW,   "show",     4},
	{ KCREATE, "create",   6},
	{ 0,        NULL,      0}
};

void
lex_init(Lex* self)
{
	self->pos          = NULL;
	self->end          = NULL;
	self->backlog_size = 0;
}

void
lex_start(Lex* self, Str* text)
{
	self->pos = str_of(text);
	self->end = str_of(text) + str_size(text);
}

void
lex_next(Lex* self, Token* tk)
{
	if (self->backlog_size > 0)
	{
		self->backlog_size--;
		*tk = self->backlog[self->backlog_size];
		return;
	}
	tk->id = KEOF;

	// skip white spaces and comments
	for (;;)
	{
		while (self->pos < self->end && isspace(*self->pos))
			self->pos++;
		// eof
		if (unlikely(self->pos == self->end))
			return;
		if (*self->pos != '#')
			break;
		while (self->pos < self->end && *self->pos != '\n')
			self->pos++;
		// eof
		if (self->pos == self->end)
			return;
	}

	// symbols
	if (*self->pos != '\"' &&
	    *self->pos != '\'' && *self->pos != '_' && ispunct(*self->pos))
	{
		tk->id = *self->pos;
		tk->integer = 0;
		self->pos++;
		return;
	}

	// integer or float
	if (isdigit(*self->pos))
	{
		tk->id = KINT;
		tk->integer = 0;
		auto start = self->pos;
		while (self->pos < self->end)
		{
			// float
			if (*self->pos == '.' || 
			    *self->pos == 'e' || 
			    *self->pos == 'E')
		   	{
				self->pos = start;
				goto reread_as_float;
			}
			if (! isdigit(*self->pos))
				break;
			tk->integer = (tk->integer * 10) + *self->pos - '0';
			self->pos++;
		}
		return;

reread_as_float:
		errno = 0;
		tk->id = KREAL;
		char* end = NULL;
		errno = 0;
		tk->real = strtod(start, &end);
		if (errno == ERANGE)
			error("config: bad float number");
		self->pos = end;
		return;
	}

	// name
	if (isalpha(*self->pos) || *self->pos == '_')
	{
		auto start = self->pos;
		while (self->pos < self->end &&
		       (*self->pos == '_' ||
	 	        *self->pos == '.' ||
	 	        *self->pos == '$' ||
	 	         isalpha(*self->pos) ||
		         isdigit(*self->pos)))
		{
			self->pos++;
		}
		str_set(&tk->string, start, self->pos - start);
		tk->id = KNAME;

		// find keyword
		auto keyword = &keywords[0];
		for (; keyword->name; keyword++)
		{
			if (keyword->name_size != str_size(&tk->string))
				continue;
			if (str_compare_raw(&tk->string, keyword->name, keyword->name_size))
			{
				tk->id = keyword->id;
				break;
			}
		}
		return;
	}

	// string
	if (*self->pos == '\"' || *self->pos == '\'')
	{
		char end_char = '"';
		if (*self->pos == '\'')
			end_char = '\'';
		self->pos++;

		auto start  = self->pos;
		bool slash  = false;
		bool escape = false;
		while (self->pos < self->end)
		{
			if (*self->pos == end_char) {
				if (slash) {
					slash = false;
					self->pos++;
					continue;
				}
				break;
			}
			if (*self->pos == '\n')
				error("config: unterminated string");
			if (*self->pos == '\\') {
				slash = !slash;
				escape = true;
			} else {
				slash = false;
			}
			self->pos++;
		}
		if (unlikely(self->pos == self->end))
			error("config: unterminated string");

		tk->id = KSTRING;
		tk->string_escape = escape;
		str_set(&tk->string, start, self->pos - start);
		self->pos++;
		return;
	}

	// error
	error("config: bad token");
}

void
lex_push(Lex* self, Token* tk)
{
	assert(self->backlog_size < 4);
	self->backlog[self->backlog_size++] = *tk;
}

bool
lex_if(Lex* self, int id, Token* tk)
{
	Token tk_;
	auto token = tk ? tk : &tk_;
	lex_next(self, token);
	if (token->id == id)
		return true;
	lex_push(self, token);
	return false;
}
