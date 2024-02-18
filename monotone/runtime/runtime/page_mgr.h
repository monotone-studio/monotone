#pragma once

//
// monotone
//
// time-series storage
//

typedef struct Page    Page;
typedef struct PageMgr PageMgr;

struct Page
{
	int   used;
	Page* next;
	char  data[];
};

struct PageMgr
{
	Spinlock lock;
	int      page_size;
	Page*    free_list;
	int      free_list_count;
};

static inline void
page_mgr_init(PageMgr* self, int page_size)
{
	self->page_size       = page_size - sizeof(Page);
	self->free_list       = NULL;
	self->free_list_count = 0;
	spinlock_init(&self->lock);
}

static inline void
page_mgr_free(PageMgr* self)
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
}

static inline Page*
page_mgr_pop(PageMgr* self)
{
	spinlock_lock(&self->lock);
	if (likely(self->free_list))
	{
		auto page = self->free_list;
		self->free_list_count--;
		self->free_list = page->next;
		spinlock_unlock(&self->lock);

		page->used = 0;
		page->next = NULL;
		return page;
	}
	spinlock_unlock(&self->lock);

	Page* page;
	page = vfs_mmap(-1, sizeof(Page) + self->page_size);
	if (unlikely(page == NULL))
		error_system();
	page->used = 0;
	page->next = NULL;
	return page;
}

static inline void
page_mgr_push(PageMgr* self, Page* page)
{
	spinlock_lock(&self->lock);
	self->free_list = page;
	self->free_list_count++;
	spinlock_unlock(&self->lock);
}

static inline void
page_mgr_push_list(PageMgr* self, Page* page, Page* page_tail, int count)
{
	spinlock_lock(&self->lock);
	page_tail->next = self->free_list;
	self->free_list = page;
	self->free_list_count += count;
	spinlock_unlock(&self->lock);
}
