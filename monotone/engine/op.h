#pragma once

//
// monotone
//
// time-series storage
//

void engine_refresh(Engine*, Compaction*, uint64_t, bool);
void engine_move(Engine*, Compaction*, uint64_t, Str*, bool);
void engine_drop(Engine*, uint64_t, bool);
void engine_rebalance(Engine*, Compaction*);
void engine_checkpoint(Engine*);
