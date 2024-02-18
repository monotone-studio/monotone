#pragma once

//
// monotone
//
// time-series storage
//

typedef struct StorageStats StorageStats;

struct StorageStats
{
	uint64_t partitions;
	uint64_t partitions_cloud;
	uint64_t min;
	uint64_t max;
	uint64_t rows;
	uint64_t rows_heap;
	uint64_t size;
	uint64_t size_uncompressed;
	uint64_t size_heap;
};

static inline void
storage_stats_init(StorageStats* self)
{
	memset(self, 0, sizeof(*self));
}

hot static inline void
storage_stats(Storage* self, StorageStats* stats)
{
	stats->partitions = self->list_count;
	stats->min        = UINT64_MAX;
	stats->max        = 0;
	list_foreach(&self->list)
	{
		auto part = list_at(Part, link);

		if (part_has(part, PART_CLOUD))
			stats->partitions_cloud++;

		// min/max
		if (part->id.min < stats->min)
			stats->min = part->id.min;
		if (part->id.max > stats->max)
			stats->max = part->id.max;

		// rows
		if (part->index)
			stats->rows += part->index->rows;
		uint64_t rows_heap = part->memtable_a.count + part->memtable_b.count;
		stats->rows_heap += rows_heap;
		stats->rows += rows_heap;

		// size
		if (part->index)
		{
			stats->size += part->index->size_regions;
			stats->size_uncompressed += part->index->size_regions_origin;
		}
		stats->size_heap += heap_used(&part->memtable_a.heap) +
		                    heap_used(&part->memtable_b.heap);
	}
}

hot static inline void
storage_stats_show(Storage* self, Buf* buf)
{
	auto source = self->source;
	StorageStats stats;
	storage_stats_init(&stats);
	storage_stats(self, &stats);
	buf_printf(buf, "%.*s\n", str_size(&source->name),
	           str_of(&source->name));
	buf_printf(buf, "  min                 %" PRIu64 "\n", stats.min);
	buf_printf(buf, "  max                 %" PRIu64 "\n", stats.max);
	buf_printf(buf, "  partitions          %" PRIu64 "\n", stats.partitions);
	buf_printf(buf, "  partitions_cloud    %" PRIu64 "\n", stats.partitions_cloud);
	buf_printf(buf, "  rows                %" PRIu64 "\n", stats.rows);
	buf_printf(buf, "  rows_heap           %" PRIu64 "\n", stats.rows_heap);

	uint64_t size = stats.size / 1024 / 1024;
	uint64_t size_uncompressed = stats.size_uncompressed / 1024 / 1024;
	uint64_t size_heap = stats.size_heap / 1024 / 1024;

	if (str_empty(&source->compression) || size == 0)
		buf_printf(buf, "  size                %" PRIu64 " Mb\n", size);
	else
		buf_printf(buf, "  size                %" PRIu64 " Mb (%.1fx compression)\n",
		           size, (float)size_uncompressed / (float)size);
	buf_printf(buf, "  size_uncompressed   %" PRIu64 " Mb\n", size_uncompressed);
	buf_printf(buf, "  size_heap           %" PRIu64 " Mb\n", size_heap);

	// source
	buf_printf(buf, "  path                '%.*s'\n", str_size(&source->path),
	           str_of(&source->path));
	buf_printf(buf, "  cloud               '%.*s'\n", str_size(&source->cloud),
	           str_of(&source->cloud));
	buf_printf(buf, "  sync                %s\n", source->sync ? "true" : "false");
	buf_printf(buf, "  crc                 %s\n", source->crc  ? "true" : "false");
	buf_printf(buf, "  compression         %.*s\n", str_size(&source->compression),
	           str_of(&source->compression));
	buf_printf(buf, "  refresh_wm          %" PRIu64 "\n", source->refresh_wm);
	buf_printf(buf, "  region_size         %" PRIu64 "\n", source->region_size);
}
