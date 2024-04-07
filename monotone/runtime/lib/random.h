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

typedef struct Random Random;

struct Random
{
	Spinlock lock;
	uint64_t seed[2];
};

void     random_init(Random*);
void     random_free(Random*);
void     random_open(Random*);
uint64_t random_generate(Random*);
void     random_generate_alnum(Random*, uint8_t*, int);
