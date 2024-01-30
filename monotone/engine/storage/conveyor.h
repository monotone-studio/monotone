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
bool  conveyor_empty(Conveyor*);
void  conveyor_alter(Conveyor*, List*);
void  conveyor_rename(Conveyor*, Str*, Str*);
void  conveyor_print(Conveyor*, Buf*);
Tier* conveyor_primary(Conveyor*);

static inline void
conveyor_validate(Conveyor* self)
{
	if (unlikely(self->storage_mgr->list_count == 0))
		error("no storages created");

	if (conveyor_empty(self) && self->storage_mgr->list_count > 1)
		error("conveyor must be set, if there are more then "
		      "one storage in the system");
}
