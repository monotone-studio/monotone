#pragma once

//
// monotone
//
// time-series storage
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
