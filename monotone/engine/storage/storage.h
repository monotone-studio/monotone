#pragma once

//
// monotone
//
// time-series storage
//

typedef struct Storage Storage;

struct Storage
{
	List    list;
	int     list_count;
	int     refs;
	Cloud*  cloud;
	Source* source;
	List    link;
};

static inline void
storage_free(Storage* self)
{
	if (self->cloud)
		cloud_free(self->cloud);
	if (self->source)
		source_free(self->source);
	mn_free(self);
}

static inline Storage*
storage_allocate(Source* source)
{
	Storage* self = mn_malloc(sizeof(Storage));
	self->cloud      = NULL;
	self->source     = NULL;
	self->refs       = 0;
	self->list_count = 0;
	list_init(&self->list);
	guard(self_guard, storage_free, self);
	self->source     = source_copy(source);
	return unguard(&self_guard);
}

static inline void
storage_ref(Storage* self)
{
	self->refs++;
}

static inline void
storage_unref(Storage* self)
{
	self->refs--;
	assert(self->refs >= 0);
}

static inline void
storage_add(Storage* self, Part* part)
{
	list_append(&self->list, &part->link);
	self->list_count++;
	part_set_cloud(part, self->cloud);
}

static inline void
storage_remove(Storage* self, Part* part)
{
	assert(part->source == self->source);
	list_unlink(&part->link);
	self->list_count--;
	part_set_cloud(part, NULL);
}

hot static inline Part*
storage_find(Storage* self, uint64_t min)
{
	list_foreach(&self->list)
	{
		auto part = list_at(Part, link);
		if (part->id.min == min)
			return part;
	}
	return NULL;
}

hot static inline Part*
storage_oldest(Storage* self)
{
	Part* min = NULL;
	list_foreach(&self->list)
	{
		auto part = list_at(Part, link);
		if (min == NULL)
			min = part;
		else
		if (part->id.psn < min->id.psn)
			min = part;
	}
	return min;
}

static inline void
storage_show_partitions(Storage* self, Buf* buf, bool verbose, bool debug)
{
	if (debug)
		verbose = true;
	buf_printf(buf, "%.*s\n", str_size(&self->source->name),
	           str_of(&self->source->name));
	list_foreach(&self->list)
	{
		auto part = list_at(Part, link);
		if (verbose)
		{
			buf_printf(buf, "  %" PRIu64 "\n", part->id.min);
			buf_printf(buf, "    min         %" PRIu64 "\n", part->id.min);
			buf_printf(buf, "    max         %" PRIu64 "\n", part->id.max);
			buf_printf(buf, "    storage     %s\n", str_of(&part->source->name));
			buf_printf(buf, "    cloud       %s\n", str_of(&part->source->cloud));
			buf_printf(buf, "    refresh     %s\n", part->refresh ? "yes" : "no");
			buf_printf(buf, "    on storage  %s\n", part_has(part, PART) ? "yes" : "no");
			buf_printf(buf, "    on cloud    %s\n", part_has(part, PART_CLOUD) ? "yes" : "no");
			buf_printf(buf, "    memtable_a  %" PRIu64 " / %" PRIu64 "\n",
			           part->memtable_a.count, part->memtable_a.size);
			buf_printf(buf, "    memtable_b  %" PRIu64 " / %" PRIu64 "\n",
			           part->memtable_b.count, part->memtable_b.size);
			buf_printf(buf, "    file        %" PRIu64 "\n", part->file.size);
			auto index = part->index;
			if (index)
			{
				buf_printf(buf, "    index\n");
				buf_printf(buf, "      size                %" PRIu64 "\n", index->size);
				buf_printf(buf, "      size_regions        %" PRIu64 "\n", index->size_regions);
				buf_printf(buf, "      size_regions_origin %" PRIu64 "\n", index->size_regions_origin);
				buf_printf(buf, "      regions             %" PRIu32 "\n", index->regions);
				buf_printf(buf, "      events              %" PRIu64 "\n", index->events);
				buf_printf(buf, "      lsn                 %" PRIu64 "\n", index->lsn);
				buf_printf(buf, "      compression         %" PRIu8  "\n", index->compression);
				if (debug)
				{
					for (uint32_t i = 0; i < index->regions; i++)
					{
						auto region = index_get(index, i);
						auto min = index_region_min(index, i);
						auto max = index_region_max(index, i);
						buf_printf(buf, "      region\n");
						buf_printf(buf, "        min             %" PRIu64 "\n", min->id);
						buf_printf(buf, "        max             %" PRIu64 "\n", max->id);
						buf_printf(buf, "        events          %" PRIu32 "\n", region->events);
						buf_printf(buf, "        offset          %" PRIu32 "\n", region->offset);
						buf_printf(buf, "        size            %" PRIu32 "\n", region->size);
					}
				}
			}

		} else
		{
			uint64_t size_cached = part->memtable_a.size + part->memtable_b.size;
			buf_printf(buf, "   %20" PRIu64" %20" PRIu64 " file size: %" PRIu64 ", size_cached: %" PRIu64 ", on storage: %s, on cloud: %s\n",
			           part->id.min, part->id.max, part->file.size, size_cached,
			           part_has(part, PART) ? "yes": "no",
			           part_has(part, PART_CLOUD) ? "yes": "no");
		}
	}
}
