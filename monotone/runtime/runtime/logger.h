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

typedef struct Logger Logger;

struct Logger
{
	bool enable;
	bool to_stdout;
	int  fd;
};

void logger_init(Logger*);
void logger_open(Logger*, const char*);
void logger_set_enable(Logger*, bool enable);
void logger_set_to_stdout(Logger*, bool enable);
void logger_close(Logger*);
void logger_write(Logger*, const char*, const char*, int, const char*,
                  const char*, va_list);
