#pragma once

//
// monotone
//
// time-series storage
//

void engine_refresh(Engine*, Refresh*, uint64_t, bool);
void engine_move(Engine*, Refresh*, uint64_t, Str*, bool);
void engine_move_range(Engine*, Refresh*, uint64_t, uint64_t, Str*);
void engine_drop(Engine*, uint64_t, bool);
void engine_drop_range(Engine*, uint64_t, uint64_t);
void engine_rebalance(Engine*, Refresh*);
void engine_checkpoint(Engine*);
