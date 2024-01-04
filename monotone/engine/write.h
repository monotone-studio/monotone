#pragma once

//
// monotone
//
// time-series storage
//

void engine_write(Engine*, bool, uint64_t, void*, int);
void engine_write_by(Engine*, EngineCursor*, bool, uint64_t, void*, int);
