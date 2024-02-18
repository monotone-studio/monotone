#pragma once

//
// monotone
//
// time-series storage
//

typedef struct Heap Heap;

struct Heap
{
	Page*    list;
	Page*    list_tail;
	int      list_count;
	PageMgr* page_mgr;
};

static inline void
heap_init(Heap* self, PageMgr* page_mgr)
{
	self->list       = NULL;
	self->list_tail  = NULL;
	self->list_count = 0;
	self->page_mgr   = page_mgr;
}

static inline void
heap_reset(Heap* self)
{
	if (self->list_count > 0)
		page_mgr_push_list(self->page_mgr, self->list, self->list_tail,
		                   self->list_count);
	self->list       = NULL;
	self->list_tail  = NULL;
	self->list_count = 0;
}

hot static inline void*
heap_allocate(Heap* self, int size)
{
	if (unlikely(size > self->page_mgr->page_size))
		return NULL;
	Page* page;
	if (unlikely(self->list == NULL ||
	             (self->page_mgr->page_size - self->list_tail->used) < size))
	{
		page = page_mgr_pop(self->page_mgr);
		if (self->list == NULL)
			self->list = page;
		else
			self->list_tail->next = page;
		self->list_tail = page;
		self->list_count++;
	} else {
		page = self->list_tail;
	}
	void* ptr = page->data + page->used;
	page->used += size;
	return ptr;
}
