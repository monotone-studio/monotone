
//
// monotone
//
// time-series storage
//

#include <monotone_runtime.h>
#include <monotone_lib.h>
#include <monotone_config.h>
#include <monotone_cloud.h>
#include <monotone_io.h>
#include <monotone_storage.h>
#include <monotone_wal.h>
#include <monotone_engine.h>
#include <monotone_command.h>

typedef struct
{
	int         id;
	const char* name;
	int         name_size;
} Keyword;

static Keyword keywords[] =
{
	// boolean
	{ KTRUE,       "true",         4},
	{ KFALSE,      "false",        5},
	// time modificators
	{ KUSEC,       "us",           2},
	{ KUSEC,       "microsecond",  11},
	{ KUSEC,       "microseconds", 12},
	{ KMSEC,       "ms",           2},
	{ KMSEC,       "millisecond",  11},
	{ KMSEC,       "milliseconds", 12},
	{ KSEC,        "sec",          3},
	{ KSEC,        "second",       6},
	{ KSEC,        "seconds",      7},
	{ KMIN,        "min",          2},
	{ KMIN,        "minute",       6},
	{ KMIN,        "minutes",      7},
	{ KHOUR,       "hr",           2},
	{ KHOUR,       "hour",         4},
	{ KHOUR,       "hours",        5},
	{ KDAY,        "day",          3},
	{ KDAY,        "days",         4},
	{ KWEEK,       "week",         4},
	{ KWEEK,       "weeks",        5},
	{ KMONTH,      "month",        5},
	{ KMONTH,      "months",       6},
	{ KYEAR,       "year",         4},
	{ KYEAR,       "years",        5},
	// size modificators
	{ KKB,         "kb",           2},
	{ KKIB,        "kib",          3},
	{ KKIB,        "k",            1},
	{ KMB,         "mb",           2},
	{ KMIB,        "mib",          3},
	{ KMIB,        "m",            1},
	{ KGB,         "gb",           2},
	{ KGIB,        "gib",          3},
	{ KGIB,        "g",            1},
	{ KTB,         "tb",           2},
	{ KTIB,        "tib",          3},
	{ KTIB,        "t",            1},
	// keywords
	{ KSET,        "set",          3},
	{ KRESET,      "reset",        5},
	{ KTO,         "to",           2},
	{ KSHOW,       "show",         4},
	{ KALL,        "all",          3},
	{ KCONFIG,     "config",       6},
	{ KCHECKPOINT, "checkpoint",   10},
	{ KSERVICE,    "service",      7},
	{ KCREATE,     "create",       6},
	{ KSTORAGE,    "storage",      7},
	{ KSTORAGES,   "storages",     8},
	{ KPARTITION,  "partition",    9},
	{ KPARTITIONS, "partitions",   10},
	{ KIF,         "if",           2},
	{ KNOT,        "not",          3},
	{ KEXISTS,     "exists",       6},
	{ KDROP,       "drop"  ,       4},
	{ KPIPELINE,   "pipeline",     8},
	{ KALTER,      "alter",        5},
	{ KRENAME,     "rename",       6},
	{ KMOVE,       "move",         4},
	{ KBETWEEN,    "between",      7},
	{ KAND,        "and",          3},
	{ KINTO,       "into",         4},
	{ KREFRESH,    "refresh",      7},
	{ KREBALANCE,  "rebalance",    9},
	{ KON,         "on",           2},
	{ KCLOUD,      "cloud",        5},
	{ KCLOUDS,     "clouds",       6},
	{ KDOWNLOAD,   "download",     8},
	{ KUPLOAD,     "upload",       6},
	{ KVERBOSE,    "verbose",      7},
	{ KDEBUG,      "debug",        5},
	{ KWAL,        "wal",          3},
	{ KMEMORY,     "memory",       6},
	{ 0,            NULL,          0}
};

void
lex_init(Lex* self)
{
	self->pos          = NULL;
	self->end          = NULL;
	self->backlog_size = 0;
	self->keywords     = true;
}

void
lex_start(Lex* self, Str* text)
{
	self->pos = str_of(text);
	self->end = str_of(text) + str_size(text);
}

void
lex_keywords(Lex* self, bool on)
{
	self->keywords = on;
}

static void
lex_integer_mod(Lex* self, Token* value)
{
	Token tk;
	lex_next(self, &tk);
	switch (tk.id) {
	// time (convert to microseconds)
	case KUSEC:
		// do nothing (default resolution)
		break;
	case KMSEC:
		value->integer *= 1000ULL;
		break;
	case KSEC:
		value->integer *= 1000ULL * 1000;
		break;
	case KMIN:
		value->integer *= 60ULL * 1000 * 1000;
		break;
	case KHOUR:
		value->integer *= 60ULL * 60 * 1000 * 1000;
		break;
	case KDAY:
		value->integer *= 24ULL * 60 * 60 * 1000 * 1000;
		break;
	case KWEEK:
		value->integer *= 7ULL * 24 * 60 * 60 * 1000 * 1000;
		break;
	case KMONTH:
		value->integer *= 30ULL * 7 * 24 * 60 * 60 * 1000 * 1000;
		break;
	case KYEAR:
		value->integer *= 12ULL * 30 * 7 * 24 * 60 * 60 * 1000 * 1000;
		break;
	// units
	case KKB:
		value->integer *= 1000ULL;
		break;
	case KKIB:
		value->integer *= 1024ULL;
		break;
	case KMB:
		value->integer *= 1000ULL * 1000;
		break;
	case KMIB:
		value->integer *= 1024ULL * 1024;
		break;
	case KGB:
		value->integer *= 1000ULL * 1000 * 1000;
		break;
	case KGIB:
		value->integer *= 1024ULL * 1024 * 1024;
		break;
	case KTB:
		value->integer *= 1000ULL * 1000 * 1000 * 1000;
		break;
	case KTIB:
		value->integer *= 1024ULL * 1024 * 1024 * 1024;
		break;
	default:
		lex_push(self, &tk);
		break;
	}
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

		// apply value modificator, if any
		lex_integer_mod(self, tk);
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

		if (! self->keywords)
			return;

		// find keyword
		auto keyword = &keywords[0];
		for (; keyword->name; keyword++)
		{
			if (keyword->name_size != str_size(&tk->string))
				continue;
			if (str_compare_case(&tk->string, keyword->name, keyword->name_size))
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

bool
lex_if_name(Lex* self, Token* tk)
{
	lex_keywords(self, false);
	bool match = lex_if(self, KNAME, tk);
	lex_keywords(self, true);
	return match;
}
