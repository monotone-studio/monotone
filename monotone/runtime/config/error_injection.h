#pragma once

//
// monotone
//
// time-series storage
//

#define error_injection(name) \
	if (unlikely(var_int_of(&config()->name))) \
		error("(%s:%d) error injection: %s", __FILE__, __LINE__, #name)
