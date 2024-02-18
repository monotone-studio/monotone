#pragma once

//
// monotone
//
// time-series storage
//

typedef struct HeapPage HeapPage;
typedef struct Heap     Heap;

struct HeapPage
{
	int       used;
	HeapPage* next;
	char      data[];
};

struct Heap
{
	int       page_size;
	HeapPage* list;
	int       list_count;
};

static inline void
heap_init(Heap* self, int page_size)
{
	self->page_size  = page_size - sizeof(HeapPage);
	self->list       = NULL;
	self->list_count = 0;
}

static inline void
heap_free(Heap* self)
{
	auto page = self->list;
	while (page)
	{
		auto next = page->next;
		vfs_munmap(page, sizeof(HeapPage) + self->page_size);
		page = next;
	}
	self->list       = NULL;
	self->list_count = 0;
}

hot static inline void*
heap_allocate(Heap* self, int size)
{
	if (unlikely(size > self->page_size))
		return NULL;
	HeapPage* page = self->list;
	if (unlikely(page == NULL || (self->page_size - page->used) < size))
	{
		page = vfs_mmap(-1, sizeof(HeapPage) + self->page_size);
		if (unlikely(page == NULL))
			error_system();
		page->used = 0;
		page->next = self->list;
		self->list = page;
		self->list_count++;
	}
	void* ptr = page->data + page->used;
	page->used += size;
	return ptr;
}
