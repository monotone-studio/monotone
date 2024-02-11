#pragma once

//
// monotone
//
// time-series storage
//

void engine_insert(Engine*, Log*, uint64_t, void*, int);
void engine_delete(Engine*, Log*, uint64_t, void*, int);
void engine_write(Engine*, Log*);
