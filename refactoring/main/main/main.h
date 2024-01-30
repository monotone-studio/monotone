#pragma once

//
// monotone
//
// time-series storage
//

typedef struct Main Main;

struct Main
{
	Engine     engine;
	Service    service;
	WorkerMgr  worker_mgr;
	Logger     logger;
	UuidMgr    uuid_mgr;
	Comparator comparator;
	Context    context;
	Config     config;
	Global     global;
};

void main_init(Main*);
void main_free(Main*);
void main_prepare(Main*);
void main_start(Main*, const char*);
void main_stop(Main*);

static inline void
main_set_runtime(Main* self)
{
	runtime_init(&self->context);
}