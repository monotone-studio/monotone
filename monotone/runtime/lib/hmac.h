#pragma once

//
// monotone
//
// time-series storage
//

#define HMAC_SZ 28

void hmac_base64(char*, int, void*, int, void*, int);
