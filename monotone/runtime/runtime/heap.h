#pragma once

//
// monotone
//
// time-series storage
//

typedef struct Heap Heap;

struct Heap
{
	Page*      list;
	Page*      list_tail;
	int        list_count;
	MemoryMgr* memory_mgr;
};

static inline void
heap_init(Heap* self, MemoryMgr* memory_mgr)
{
	self->list       = NULL;
	self->list_tail  = NULL;
	self->list_count = 0;
	self->memory_mgr = memory_mgr;
}

static inline void
heap_reset(Heap* self)
{
	if (self->list_count > 0)
		memory_mgr_push_list(self->memory_mgr,
		                     self->list,
		                     self->list_tail,
		                     self->list_count);
	self->list       = NULL;
	self->list_tail  = NULL;
	self->list_count = 0;
}

hot static inline void*
heap_allocate(Heap* self, int size)
{
	if (unlikely(size > self->memory_mgr->page_size))
		return NULL;
	Page* page;
	if (unlikely(self->list == NULL ||
	             (self->memory_mgr->page_size - self->list_tail->used) < size))
	{
		page = memory_mgr_pop(self->memory_mgr);
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
