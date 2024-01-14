#pragma once

//
// monotone
//
// time-series storage
//

typedef struct Conveyor Conveyor;

struct Conveyor
{
	List        list;
	int         list_count;
	StorageMgr* storage_mgr;
};

void  conveyor_init(Conveyor*, StorageMgr*);
void  conveyor_free(Conveyor*);
void  conveyor_open(Conveyor*);
bool  conveyor_exists(Conveyor*);
void  conveyor_create(Conveyor*, List*, bool);
void  conveyor_drop(Conveyor*, bool);
void  conveyor_print(Conveyor*, Buf*);
Tier* conveyor_primary(Conveyor*);
