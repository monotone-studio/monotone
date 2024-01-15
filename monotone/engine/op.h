#pragma once

//
// monotone
//
// time-series storage
//

// storage
void engine_storage_create(Engine*, Target*, bool);
void engine_storage_drop(Engine*, Str*, bool);
void engine_storage_show(Engine*, Str*, Buf*);
void engine_storage_show_all(Engine*, Buf*);

// conveyor
void engine_conveyor_alter(Engine*, List*);
void engine_conveyor_show(Engine*, Buf*);

// partitions
void engine_partitions_drop(Engine*, uint64_t, uint64_t);
void engine_partitions_move(Engine*, uint64_t, uint64_t, Str*);
void engine_partitions_show(Engine*, Buf*, Str*);
void engine_checkpoint(Engine*);
