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

typedef struct Json Json;

struct Json
{
	const char* json;
	int         json_size;
	int         pos;
	Buf         buf;
};

void json_init(Json*);
void json_free(Json*);
void json_reset(Json*);
void json_parse(Json*, Str*);
void json_export(Buf*, uint8_t**);
void json_export_pretty(Buf*, uint8_t**);
