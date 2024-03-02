#pragma once

//
// monotone
//
// time-series storage
//

enum
{
	ENGINE_SHOW_STORAGES,
	ENGINE_SHOW_PARTITIONS
};

void engine_show(Engine*, int, Str*, Buf*, bool);
