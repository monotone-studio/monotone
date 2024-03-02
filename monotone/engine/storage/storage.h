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
	int64_t size;
	int64_t events;
	Cloud*  cloud;
	Source* source;
	List    link;
};

static inline void
storage_free(Storage* self)
{
	if (self->cloud)
		cloud_unref(self->cloud);
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
	self->size       = 0;
	self->events     = 0;
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
storage_add_metrics(Storage* self, Part* part)
{
	if (! part->index)
		return;
	self->size += part->index->size_regions;
	self->events += part->index->events;
}

static inline void
storage_remove_metrics(Storage* self, Part* part)
{
	if (! part->index)
		return;
	self->size -= part->index->size_regions;
	self->events -= part->index->events;
}

static inline void
storage_add(Storage* self, Part* part)
{
	list_append(&self->list, &part->link);
	self->list_count++;
	storage_add_metrics(self, part);
	part_set_cloud(part, self->cloud);
}

static inline void
storage_remove(Storage* self, Part* part)
{
	assert(part->source == self->source);
	list_unlink(&part->link);
	self->list_count--;
	part_set_cloud(part, NULL);
	storage_remove_metrics(self, part);
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

hot static inline void
storage_show(Storage* self, Buf* buf, bool debug)
{
	// gather storage stats
	Stats stats;
	stats_init(&stats);
	list_foreach(&self->list)
	{
		auto part = list_at(Part, link);
		stats_add(&stats, part);
	}

	// map
	encode_map(buf, 2);

	// source config
	encode_raw(buf, "config", 6);
	source_write(self->source, buf, debug);

	// stats
	encode_raw(buf, "stats", 5);
	stats_show(&stats, buf);
}
