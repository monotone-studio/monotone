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

typedef struct Part Part;

struct Part
{
	Id          id;
	int         state;
	uint64_t    time_create;
	uint64_t    time_refresh;
	bool        refresh;
	Memtable*   memtable;
	Memtable    memtable_a;
	Memtable    memtable_b;
	File        file;
	Index       index;
	Buf         index_data;
	Cloud*      cloud;
	Source*     source;
	Comparator* comparator;
	List        link;
};

Part*
part_allocate(Comparator*, Source*, Id*);
void part_free(Part*);
void part_open(Part*, int, bool);
void part_create(Part*, int);
void part_delete(Part*, int);
void part_rename(Part*, int, int);
void part_download(Part*);
void part_upload(Part*);
void part_offload(Part*, bool);

static inline void
part_set(Part* self, int state)
{
	self->state |= state;
}

static inline void
part_unset(Part* self, int state)
{
	self->state &= ~state;
}

static inline bool
part_has(Part* self, int mask)
{
	return (self->state & mask) > 0;
}

static inline void
part_set_time_create(Part* self, uint64_t time)
{
	self->time_create = time;
}

static inline void
part_set_time_refresh(Part* self, uint64_t time)
{
	self->time_refresh = time;
}

static inline void
part_set_cloud(Part* self, Cloud* cloud)
{
	self->cloud = cloud;
}

static inline Memtable*
part_memtable(Part* self, Memtable** prev)
{
	if (self->memtable == &self->memtable_a)
		*prev = &self->memtable_b;
	else
		*prev = &self->memtable_a;
	return self->memtable;
}

static inline Memtable*
part_memtable_rotate(Part* self)
{
	Memtable* prev = self->memtable;
	Memtable* current;
	if (prev == &self->memtable_a)
		current = &self->memtable_b;
	else
		current = &self->memtable_a;
	self->memtable = current;
	return prev;
}

static inline bool
part_refresh_ready(Part* self)
{
	if (self->refresh)
		return false;
	if (! self->source->refresh_wm)
		return false;
	return heap_used(&self->memtable->heap) > (uint32_t)self->source->refresh_wm;
}
