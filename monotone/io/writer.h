#pragma once

//
// monotone
//
// time-series storage
//

typedef struct Writer Writer;

struct Writer
{
	File*        file;
	Iov          iov;
	RegionWriter region_writer;
	IndexWriter  index_writer;
	Target*      target;
};

void writer_init(Writer*);
void writer_free(Writer*);
void writer_reset(Writer*);
void writer_start(Writer*, Target*, File*);
void writer_stop(Writer*, uint64_t, bool);
void writer_add(Writer*, Row*);
