#pragma once

//
// monotone
//
// time-series storage
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
void main_prepare(Main*, Compare, void*);
void main_start(Main*, const char*);
void main_stop(Main*);
