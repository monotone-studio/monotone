#pragma once

//
// monotone
//
// time-series storage
//

typedef struct Config Config;

struct Config
{
	// main
	Var  version;
	Var  uuid;
	Var  online;
	Var  directory;
	// log
	Var  log_enable;
	Var  log_to_file;
	Var  log_to_stdout;
	// engine
	Var  interval;
	Var  workers;
	// state
	Var  storages;
	// testing
	Var  test_bool;
	Var  test_int;
	Var  test_string;
	Var  test_data;
	List list;
	List list_persistent;
	int  count;
	int  count_visible;
	int  count_config;
	int  count_persistent;
};

void config_init(Config*);
void config_free(Config*);
void config_prepare(Config*);
void config_open(Config*, const char*);
void config_save(Config*, const char*);
void config_print_log(Config*);
void config_print(Config*, Buf*);
Var* config_find(Config*, Str*);
