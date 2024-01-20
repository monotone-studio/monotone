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
	uint64_t min;
	uint64_t max;
	uint64_t rows;
	uint64_t rows_cached;
	uint64_t size;
	uint64_t size_uncompressed;
	uint64_t size_cached;
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

		// min/max
		if (part->min < stats->min)
			stats->min = part->min;
		if (part->max > stats->max)
			stats->max = part->max;

		// rows
		if (part->index)
			stats->rows += part->index->count_total;
		uint64_t rows_cached = part->memtable_a.count + part->memtable_b.count;
		stats->rows_cached += rows_cached;
		stats->rows += rows_cached;

		// size
		if (part->index)
		{
			stats->size += part->index->size_total;
			stats->size_uncompressed += part->index->size_total_origin;
		}
		uint64_t size_cached = part->memtable_a.size + part->memtable_b.size;
		stats->size_cached += size_cached;
		stats->size += size_cached;
	}
}

hot static inline void
storage_stats_show(Storage* self, Buf* buf)
{
	auto target = self->target;
	StorageStats stats;
	storage_stats_init(&stats);
	storage_stats(self, &stats);
	buf_printf(buf, "%.*s\n", str_size(&target->name),
	           str_of(&target->name));
	buf_printf(buf, "  partitions        %" PRIu64 "\n", stats.partitions);
	buf_printf(buf, "  min               %" PRIu64 "\n", stats.min);
	buf_printf(buf, "  max               %" PRIu64 "\n", stats.max);
	buf_printf(buf, "  rows              %" PRIu64 "\n", stats.rows);
	buf_printf(buf, "  rows_cached       %" PRIu64 "\n", stats.rows_cached);
	buf_printf(buf, "  size              %" PRIu64 " Mb\n", stats.size / 1024 / 1024);
	buf_printf(buf, "  size_uncompressed %" PRIu64 " Mb\n", stats.size_uncompressed / 1024 / 1024);
	buf_printf(buf, "  size_cached       %" PRIu64 " Mb\n", stats.size_cached / 1024 / 1024);
	// target
	buf_printf(buf, "  path              '%.*s'\n", str_size(&target->path),
	           str_of(&target->path));
	buf_printf(buf, "  sync              %s\n", target->sync ? "true" : "false");
	buf_printf(buf, "  crc               %s\n", target->sync ? "true" : "false");
	buf_printf(buf, "  compression       %" PRIu64 "\n", target->compression);
	buf_printf(buf, "  refresh_wm        %" PRIu64 "\n", target->refresh_wm);
	buf_printf(buf, "  region_size       %" PRIu64 "\n", target->region_size);
}
