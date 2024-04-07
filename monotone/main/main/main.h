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

typedef struct Main Main;

struct Main
{
	Rwlock         lock;
	File           lock_directory;
	Engine         engine;
	Wal            wal;
	Service        service;
	WorkerMgr      worker_mgr;
	CloudMgr       cloud_mgr;
	MemoryMgr      memory_mgr;
	CompressionMgr compression_mgr;
	EncryptionMgr  encryption_mgr;
	Random         random;
	Comparator     comparator;
	Logger         logger;
	Context        context;
	Control        control;
	Config         config;
	Global         global;
};

void main_init(Main*);
void main_free(Main*);
void main_prepare(Main*);
void main_set_compare(Main*, Compare, void*);
void main_start(Main*, const char*);
void main_stop(Main*);
