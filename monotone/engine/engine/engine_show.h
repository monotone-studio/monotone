#pragma once

//
// monotone.
//
// embeddable cloud-native storage for events
// and time-series data.
//
// Copyright (c) 2023-2024 Dmitry Simonenko
// Copyright (c) 2023-2024 Monotone
//
// Distributed under the terms of GNU LGPL v3 or
// Monotone Commercial License.
//

enum
{
	ENGINE_SHOW_STORAGES,
	ENGINE_SHOW_PARTITION,
	ENGINE_SHOW_PARTITIONS
};

void engine_show(Engine*, int, Str*, uint64_t, Buf*, bool);
