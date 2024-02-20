#pragma once

//
// monotone
//
// time-series storage
//

typedef struct Wal Wal;

struct Wal
{
	Mutex    lock;
	WalId    list;
	WalFile* current;
};

void wal_init(Wal*);
void wal_free(Wal*);
void wal_open(Wal*);
void wal_rotate(Wal*, uint64_t);
void wal_gc(Wal*, uint64_t);
bool wal_write(Wal*, Log*);
bool wal_write_op(Wal*, LogWrite*);
void wal_show(Wal*, Buf*);
