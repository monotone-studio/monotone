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

static inline uint64_t
time_us(void)
{
	struct timespec t;
	clock_gettime(CLOCK_REALTIME, &t);
	uint64_t time_ns = t.tv_sec * (uint64_t)1e9 + t.tv_nsec;
	// us
	return time_ns / 1000;
}
