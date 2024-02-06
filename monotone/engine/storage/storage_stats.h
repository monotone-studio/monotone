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
	uint64_t partitions_in_cloud;
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

		if (part_has(part, PART_FILE_CLOUD))
			stats->partitions_in_cloud++;

		// min/max
		if (part->id.min < stats->min)
			stats->min = part->id.min;
		if (part->id.max > stats->max)
			stats->max = part->id.max;

		// rows
		if (part->index)
			stats->rows += part->index->rows;
		uint64_t rows_cached = part->memtable_a.count + part->memtable_b.count;
		stats->rows_cached += rows_cached;
		stats->rows += rows_cached;

		// size
		if (part->index)
		{
			stats->size += part->index->size_regions;
			stats->size_uncompressed += part->index->size_regions_origin;
		}
		uint64_t size_cached = part->memtable_a.size + part->memtable_b.size;
		stats->size_cached += size_cached;
		stats->size += size_cached;
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
	buf_printf(buf, "  partitions in cloud %" PRIu64 "\n", stats.partitions_in_cloud);
	buf_printf(buf, "  rows                %" PRIu64 "\n", stats.rows);
	buf_printf(buf, "  rows_cached         %" PRIu64 "\n", stats.rows_cached);
	buf_printf(buf, "  size                %" PRIu64 " Mb\n", stats.size / 1024 / 1024);
	buf_printf(buf, "  size_uncompressed   %" PRIu64 " Mb\n", stats.size_uncompressed / 1024 / 1024);
	buf_printf(buf, "  size_cached         %" PRIu64 " Mb\n", stats.size_cached / 1024 / 1024);

	// source
	buf_printf(buf, "  path                '%.*s'\n", str_size(&source->path),
	           str_of(&source->path));
	buf_printf(buf, "  cloud               '%.*s'\n", str_size(&source->cloud),
	           str_of(&source->cloud));
	buf_printf(buf, "  sync                %s\n", source->sync ? "true" : "false");
	buf_printf(buf, "  crc                 %s\n", source->crc  ? "true" : "false");
	buf_printf(buf, "  compression         %" PRIu64 "\n", source->compression);
	buf_printf(buf, "  refresh_wm          %" PRIu64 "\n", source->refresh_wm);
	buf_printf(buf, "  region_size         %" PRIu64 "\n", source->region_size);
}
