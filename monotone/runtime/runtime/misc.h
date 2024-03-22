#pragma once

//
// monotone
//
// time-series storage
//

always_inline static inline int64_t
compare_u64(uint64_t a, uint64_t b)
{
	return (int64_t)a - (int64_t)b;
}
