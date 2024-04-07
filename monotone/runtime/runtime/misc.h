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

always_inline static inline int64_t
compare_u64(uint64_t a, uint64_t b)
{
	return (int64_t)a - (int64_t)b;
}
