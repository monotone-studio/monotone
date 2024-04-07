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

typedef struct Page      Page;
typedef struct MemoryMgr MemoryMgr;

struct Page
{
	int   used;
	Page* next;
	char  data[];
};

struct MemoryMgr
{
	Mutex    lock;
	CondVar  cond_var;
	int      count;
	Page*    free_list;
	int      free_list_count;
	int      page_size;
	uint64_t cache;
	bool     limit;
	bool     limit_error;
	uint64_t limit_wm;
	uint64_t limit_hits;
};

static inline void
memory_mgr_init(MemoryMgr* self)
{
	self->count           = 0;
	self->free_list       = NULL;
	self->free_list_count = 0;
	self->page_size       = 2097152 - sizeof(Page);
	self->cache           = 8589934592; // 8GiB
	self->limit           = false;
	self->limit_error     = false;
	self->limit_wm        = 8589934592; // 8GiB
	self->limit_hits      = 0;
	mutex_init(&self->lock);
	cond_var_init(&self->cond_var);
}

static inline void
memory_mgr_set(MemoryMgr* self, int page_size,
               uint64_t   cache,
               bool       limit,
               Str*       limit_behaviour,
               uint64_t   limit_wm)
{
	self->page_size = page_size - sizeof(Page);
	self->cache     = cache;
	self->limit     = limit;
	self->limit_wm  = limit_wm;
	if (str_compare_raw(limit_behaviour, "block", 5))
		self->limit_error = false;
	else
	if (str_compare_raw(limit_behaviour, "error", 5))
		self->limit_error = true;
	else
		error("mm: unrecognized memory limit behaviour");
}

static inline void
memory_mgr_reset(MemoryMgr* self)
{
	mutex_lock(&self->lock);
	auto page = self->free_list;
	self->count          -= self->free_list_count;
	self->free_list       = NULL;
	self->free_list_count = 0;
	mutex_unlock(&self->lock);

	while (page)
	{
		auto next = page->next;
		vfs_munmap(page, sizeof(Page) + self->page_size);
		page = next;
	}
}

static inline void
memory_mgr_free(MemoryMgr* self)
{
	auto page = self->free_list;
	while (page)
	{
		auto next = page->next;
		vfs_munmap(page, sizeof(Page) + self->page_size);
		page = next;
	}
	self->free_list       = NULL;
	self->free_list_count = 0;

	mutex_free(&self->lock);
	cond_var_free(&self->cond_var);
}

static inline Page*
memory_mgr_pop_cache(MemoryMgr* self)
{
	assert(self->free_list);
	auto page = self->free_list;
	self->free_list_count--;
	self->free_list = page->next;
	page->used = 0;
	page->next = NULL;
	return page;
}

static inline Page*
memory_mgr_pop(MemoryMgr* self)
{
	mutex_lock(&self->lock);

	if (likely(self->free_list))
	{
		auto page = memory_mgr_pop_cache(self);
		mutex_unlock(&self->lock);
		return page;
	}

	if (self->limit)
	{
		uint64_t used = self->count * (uint64_t)self->page_size;
		if (unlikely(used >= self->limit_wm))
		{
			self->limit_hits++;
			if (self->limit_error)
			{
				mutex_unlock(&self->lock);
				error("mm: memory limit reached");
			}

			// wait for free page
			while (! self->free_list)
				cond_var_wait(&self->cond_var, &self->lock);

			auto page = memory_mgr_pop_cache(self);
			mutex_unlock(&self->lock);
			return page;
		}
	}

	self->count++;
	mutex_unlock(&self->lock);

	Page* page;
	page = vfs_mmap(-1, sizeof(Page) + self->page_size);
	if (unlikely(page == NULL))
		error_system();
	page->used = 0;
	page->next = NULL;
	return page;
}

static inline void
memory_mgr_push(MemoryMgr* self, Page* page, Page* page_tail, int count)
{
	mutex_lock(&self->lock);
	uint64_t cache = self->free_list_count * (uint64_t)self->page_size;
	if (cache < self->cache)
	{
		page_tail->next = self->free_list;
		self->free_list = page;
		self->free_list_count += count;
		cond_var_broadcast(&self->cond_var);
		page = NULL;
	} else {
		self->count -= count;
	}
	mutex_unlock(&self->lock);

	while (page)
	{
		auto next = page->next;
		vfs_munmap(page, sizeof(Page) + self->page_size);
		page = next;
	}
}

static inline void
memory_mgr_show(MemoryMgr* self, Buf* buf)
{
	mutex_lock(&self->lock);
	int      pages       = self->count;
	int      pages_used  = self->count - self->free_list_count;
	int      pages_free  = self->free_list_count;
	int      page_size   = self->page_size + sizeof(Page);
	uint64_t cache       = self->cache;
	bool     limit       = self->limit;
	uint64_t limit_wm    = self->limit_wm;
	bool     limit_error = self->limit_error;
	uint64_t limit_hits  = self->limit_hits;
	mutex_unlock(&self->lock);

	// map
	encode_map(buf, 9);

	// pages
	encode_raw(buf, "pages", 5);
	encode_integer(buf, pages);

	// pages_used
	encode_raw(buf, "pages_used", 10);
	encode_integer(buf, pages_used);

	// pages_free
	encode_raw(buf, "pages_free", 10);
	encode_integer(buf, pages_free);

	// page_size
	encode_raw(buf, "page_size", 9);
	encode_integer(buf, page_size);

	// limit
	encode_raw(buf, "limit", 5);
	encode_bool(buf, limit);

	// limit_wm
	encode_raw(buf, "limit_wm", 8);
	encode_integer(buf, limit_wm);

	// limit_behaviour
	encode_raw(buf, "limit_behaviour", 15);
	if (limit_error)
		encode_raw(buf, "error", 5);
	else
		encode_raw(buf, "block", 5);

	// limit_hits
	encode_raw(buf, "limit_hits", 10);
	encode_integer(buf, limit_hits);

	// cache
	encode_raw(buf, "cache", 5);
	encode_integer(buf, cache);
}
