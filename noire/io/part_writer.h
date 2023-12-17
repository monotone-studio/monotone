#pragma once

//
// noire
//
// time-series storage
//

typedef struct PartWriter PartWriter;

struct PartWriter
{
	Part*        part;
	Iov          iov;
	RegionWriter region_writer;
	IndexWriter  index_writer;
	int          compression;
	bool         crc;
	int          size_region;
};

void part_writer_init(PartWriter*);
void part_writer_free(PartWriter*);
void part_writer_reset(PartWriter*);
void part_writer_start(PartWriter*, Comparator*, uint64_t, uint64_t, int, bool, int);
void part_writer_stop(PartWriter*, uint64_t);
void part_writer_add(PartWriter*, Row*);
