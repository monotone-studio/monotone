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

typedef struct Control Control;

struct Control
{
	void  (*save_config)(void*);
	void  (*lock)(void*, bool);
	void  (*unlock)(void*);
	void*  arg;
};

static inline void
control_init(Control* self)
{
	memset(self, 0, sizeof(*self));
}
