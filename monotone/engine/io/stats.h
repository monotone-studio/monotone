#pragma once

//
// monotone
//
// time-series part
//

typedef struct Stats Stats;

struct Stats
{
	uint64_t partitions;
	uint64_t partitions_local;
	uint64_t partitions_cloud;
	uint64_t min;
	uint64_t max;
	uint64_t events;
	uint64_t events_heap;
	uint64_t size;
	uint64_t size_uncompressed;
	uint64_t size_heap;
};

static inline void
stats_init(Stats* self)
{
	memset(self, 0, sizeof(*self));
	self->min = UINT64_MAX;
	self->max = 0;
}

static inline void
stats_add(Stats* self, Part* part)
{
	// partitions
	self->partitions++;
	if (part_has(part, PART))
		self->partitions_local++;
	if (part_has(part, PART_CLOUD))
		self->partitions_cloud++;

	// min/max
	if (part->id.min < self->min)
		self->min = part->id.min;

	if (part->id.max > self->max)
		self->max = part->id.max;

	// events
	if (part->index)
		self->events += part->index->events;
	uint64_t events_heap = atomic_u64_of(&part->memtable_a.count) +
	                       atomic_u64_of(&part->memtable_b.count);
	self->events_heap += events_heap;
	self->events += events_heap;

	// size
	if (part->index)
	{
		self->size += part->index->size + part->index->size_regions;
		self->size_uncompressed += part->index->size + part->index->size_regions_origin;
	}
	self->size_heap += heap_used(&part->memtable_a.heap) +
	                   heap_used(&part->memtable_b.heap);
}

static inline void
stats_show(Stats* self, Buf* buf)
{
	// {}
	encode_map(buf, 10);

	// min
	encode_raw(buf, "min", 3);
	encode_integer(buf, self->min);

	// max
	encode_raw(buf, "max", 3);
	encode_integer(buf, self->max);

	// partitions
	encode_raw(buf, "partitions", 10);
	encode_integer(buf, self->partitions);

	// partitions_local
	encode_raw(buf, "partitions_local", 16);
	encode_integer(buf, self->partitions_local);

	// partitions_cloud
	encode_raw(buf, "partitions_cloud", 16);
	encode_integer(buf, self->partitions_cloud);

	// events
	encode_raw(buf, "events", 6);
	encode_integer(buf, self->events);

	// events_heap
	encode_raw(buf, "events_heap", 11);
	encode_integer(buf, self->events_heap);

	// size
	encode_raw(buf, "size", 4);
	encode_integer(buf, self->size);

	// size_uncompressed
	encode_raw(buf, "size_uncompressed", 17);
	encode_integer(buf, self->size_uncompressed);

	// size_heap
	encode_raw(buf, "size_heap", 9);
	encode_integer(buf, self->size_heap);
}

static inline void
stats_show_part(Part* self, Buf* buf, bool debug)
{
	Stats stats;
	stats_init(&stats);
	stats_add(&stats, self);

	// {}
	if (debug)
		encode_map(buf, 11);
	else
		encode_map(buf, 10);

	// stats
	
	// min
	encode_raw(buf, "min", 3);
	encode_integer(buf, self->id.min);

	// max
	encode_raw(buf, "max", 3);
	encode_integer(buf, self->id.max);

	// events
	encode_raw(buf, "events", 6);
	encode_integer(buf, stats.events);

	// events_heap
	encode_raw(buf, "events_heap", 11);
	encode_integer(buf, stats.events_heap);

	// size
	encode_raw(buf, "size", 4);
	encode_integer(buf, stats.size);

	// size_uncompressed
	encode_raw(buf, "size_uncompressed", 17);
	encode_integer(buf, stats.size_uncompressed);

	// size_heap
	encode_raw(buf, "size_heap", 9);
	encode_integer(buf, stats.size_heap);

	// on_storage
	encode_raw(buf, "on_storage", 10);
	encode_bool(buf, part_has(self, PART));

	// on_cloud
	encode_raw(buf, "on_cloud", 8);
	encode_bool(buf, part_has(self, PART_CLOUD));

	// storage
	encode_raw(buf, "storage", 7);
	encode_string(buf, &self->source->name);

	// index
	if (debug)
	{
		encode_raw(buf, "index", 5);

		auto index = self->index;
		if (index)
		{
			// {}
			encode_map(buf, 7);

			// size
			encode_raw(buf, "size", 4);
			encode_integer(buf, index->size);

			// size_regions
			encode_raw(buf, "size_regions", 12);
			encode_integer(buf, index->size_regions);

			// size_regions_origin
			encode_raw(buf, "size_regions_origin", 19);
			encode_integer(buf, index->size_regions_origin);

			// regions
			encode_raw(buf, "regions", 7);
			encode_integer(buf, index->regions);

			// events
			encode_raw(buf, "events", 6);
			encode_integer(buf, index->events);

			// lsn
			encode_raw(buf, "lsn", 3);
			encode_integer(buf, index->lsn);

			// compression
			encode_raw(buf, "compression", 11);
			encode_integer(buf, index->compression);
		} else
		{
			encode_null(buf);
		}
	}
}