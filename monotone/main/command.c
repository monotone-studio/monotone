
//
// monotone
//
// time-series storage
//

#include <monotone_runtime.h>
#include <monotone_lib.h>
#include <monotone_io.h>
#include <monotone_storage.h>
#include <monotone_main.h>

static void
execute_set(Lex* lex)
{
	// SET name TO INT|STRING
	Token name;
	if (! lex_if(lex, KNAME, &name))
		error("SET <name> expected");
	// TO
	if (! lex_if(lex, KTO, NULL))
		error("SET name <TO> expected");

	// value
	Token value;
	lex_next(lex, &value);

	// find variable
	auto var = config_find(config(), &name.string);
	if (var && var_is(var, VAR_S))
		var = NULL;
	if (unlikely(var == NULL))
		error("SET '%.*s': variable not found", str_size(&name.string),
		      str_of(&name.string));

	// check if the variable can be updated
	if (config_online())
	{
		if (unlikely(! var_is(var, VAR_R)))
			error("SET '%.*s': variable cannot be changed online",
			      str_size(&name.string), str_of(&name.string));
	} else
	{
		if (unlikely(! var_is(var, VAR_C)))
			error("SET '%.*s': variable cannot be changed", str_size(&name.string),
			      str_of(&name.string));
	}

	// set value
	switch (var->type) {
	case VAR_BOOL:
	{
		if (value.id != KTRUE && value.id != KFALSE)
			error("SET '%.*s': bool value expected", str_size(&name.string),
			      str_of(&name.string));
		bool is_true = value.id == KTRUE;
		var_int_set(var, is_true);
		break;
	}
	case VAR_INT:
	{
		if (value.id != KINT)
			error("SET '%.*s': integer value expected", str_size(&name.string),
			      str_of(&name.string));
		var_int_set(var, value.integer);
		break;
	}
	case VAR_STRING:
	{
		if (value.id != KSTRING)
			error("SET '%.*s': string value expected", str_size(&name.string),
			      str_of(&name.string));
		var_string_set(var, &value.string);
		break;
	}
	case VAR_DATA:
	{
		error("SET '%.*s': variable cannot be changed", str_size(&name.string),
		      str_of(&name.string));
		break;
	}
	}

	// rewrite config file
	if (config_online() && !var_is(var, VAR_E))
		config_update();
}

static void
execute_show(Lex* lex, Buf* output)
{
	// SHOW [ALL | name]
	Token name;
	if (! lex_if(lex, KNAME, &name))
		error("SHOW <name> expected");

	// all
	if (str_compare_raw(&name.string, "all", 3))
	{
		config_print(config(), output);
		return;
	}

	// SHOW name
	auto var = config_find(config(), &name.string);
	if (var && var_is(var, VAR_S))
		var = NULL;
	if (unlikely(var == NULL))
		error("SHOW name: '%.*s' not found", str_size(&name.string),
		      str_of(&name.string));
	var_print_value(var, output);
}

void
main_execute(Main* self, const char* command, char** result)
{
	Buf output;
	buf_init(&output);
	guard(guard, buf_free, &output);

	Str text;
	str_set_cstr(&text, command);

	Lex lex;
	lex_init(&lex);
	lex_start(&lex, &text);

	if (result)
		*result = NULL;

	Token tk;
	lex_next(&lex, &tk);
	switch (tk.id) {
	case KSET:
		execute_set(&lex);
		break;
	case KSHOW:
		execute_show(&lex, &output);
		break;
	case KEOF:
		break;
	case KCREATE:
		(void)self;
		break;
	default:
		error("unknown command");
		break;
	}

	if (result && buf_size(&output) > 0)
	{
		buf_write(&output, "\0", 1);
		unguard(&guard);
		*result = buf_cstr(&output);
	}
}
