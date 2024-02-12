#pragma once

//
// monotone
//
// time-series storage
//

typedef struct Main Main;

struct Main
{
	Rwlock     lock;
	Engine     engine;
	Wal        wal;
	Service    service;
	WorkerMgr  worker_mgr;
	CloudMgr   cloud_mgr;
	CloudMock  cloud_mock;
	Logger     logger;
	UuidMgr    uuid_mgr;
	Comparator comparator;
	Error      error;
	Context    context;
	Control    control;
	Config     config;
	Global     global;
};

void main_init(Main*);
void main_free(Main*);
void main_prepare(Main*);
void main_start(Main*, const char*);
void main_stop(Main*);
