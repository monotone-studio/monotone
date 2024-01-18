#pragma once

//
// monotone
//
// time-series storage
//

typedef struct Engine Engine;

struct Engine
{
	Catalog     catalog;
	Service     service;
	Comparator* comparator;
};

void engine_init(Engine*, Comparator*);
void engine_free(Engine*);
void engine_open(Engine*);
void engine_close(Engine*);
