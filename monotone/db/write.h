#pragma once

//
// monotone
//
// time-series storage
//

void db_write(Db*, bool, uint64_t, void*, int);
void db_write_by(Db*, DbCursor*, bool, uint64_t, void*, int);
