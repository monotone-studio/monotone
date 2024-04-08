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

typedef struct Config Config;

struct Config
{
	// main
	Var  version;
	Var  uuid;
	Var  directory;
	Var  online;
	Var  sync;
	// log
	Var  log;
	Var  log_to_file;
	Var  log_to_stdout;
	// memory manager
	Var  mm_page_size;
	Var  mm_limit;
	Var  mm_limit_wm;
	Var  mm_limit_behaviour;
	Var  mm_cache;
	// wal
	Var  wal;
	Var  wal_rotate_wm;
	Var  wal_sync_on_rotate;
	Var  wal_sync_on_write;
	Var  wal_crc;
	// engine
	Var  serial;
	Var  interval;
	Var  workers;
	Var  workers_upload;
	// state
	Var  ssn;
	Var  lsn;
	Var  clouds;
	Var  storages;
	Var  pipeline;
	// testing
	Var  error_refresh_1;
	Var  error_refresh_2;
	Var  error_refresh_3;
	Var  error_download;
	Var  error_upload;
	Var  error_wal;
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
void config_print(Config*);
void config_show(Config*, Buf*);
Var* config_find(Config*, Str*);
