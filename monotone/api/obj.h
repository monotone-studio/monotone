#pragma once

//
// monotone
//
// time-series storage
//

enum
{
	MONOTONE_OBJ        = 0x3fb15941,
	MONOTONE_OBJ_CURSOR = 0x143BAF02,
	MONOTONE_OBJ_FREED  = 0x28101019
};

struct monotone
{
	int      type;
	Instance instance;
};

struct monotone_cursor
{
	int              type;
	EngineCursor     cursor;
	struct monotone* env;
};
