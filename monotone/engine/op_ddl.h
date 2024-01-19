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
void engine_storage_show_partitions(Engine*, Str*, Buf*);

// conveyor
void engine_conveyor_alter(Engine*, List*);
void engine_conveyor_show(Engine*, Buf*);
