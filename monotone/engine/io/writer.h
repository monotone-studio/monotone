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

typedef struct Writer Writer;

struct Writer
{
	File*        file;
	Iov          iov;
	RegionWriter region_writer;
	IndexWriter  index_writer;
	Compression* compression;
	Encryption*  encryption;
	Source*      source;
};

void writer_init(Writer*);
void writer_free(Writer*);
void writer_reset(Writer*);
void writer_start(Writer*, Source*, File*);
void writer_stop(Writer*, Id*, uint32_t, uint64_t, uint64_t, uint64_t, bool);
void writer_add(Writer*, Event*);
