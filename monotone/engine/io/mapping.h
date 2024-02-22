#pragma once

//
// monotone
//
// time-series storage
//

typedef struct Slice   Slice;
typedef struct Mapping Mapping;

struct Slice
{
	uint64_t   min;
	uint64_t   max;
	RbtreeNode node;
};

struct Mapping
{
	Rbtree      tree;
	int         tree_count;
	Comparator* comparator;
};

static inline void
slice_init(Slice* self, uint64_t min, uint64_t max)
{
	self->min = min;
	self->max = max;
	rbtree_init_node(&self->node);
}

always_inline static inline bool
slice_in(Slice* self, uint64_t min)
{
	return self->min <= min && min < self->max;
}

static inline void
mapping_init(Mapping* self, Comparator* comparator)
{
	self->tree_count = 0;
	self->comparator = comparator;
	rbtree_init(&self->tree);
}

always_inline static inline Slice*
mapping_of(RbtreeNode* node)
{
	return container_of(node, Slice, node);
}

static inline Slice*
mapping_min(Mapping* self)
{
	if (self->tree_count == 0)
		return NULL;
	auto min = rbtree_min(&self->tree);
	return mapping_of(min);
}

static inline Slice*
mapping_max(Mapping* self)
{
	if (self->tree_count == 0)
		return NULL;
	auto max = rbtree_max(&self->tree);
	return mapping_of(max);
}

always_inline static inline int
mapping_compare(uint64_t a, uint64_t b)
{
	if (a == b)
		return 0;
	return (a > b) ? 1 : -1;
}

hot static inline
rbtree_get(mapping_find, mapping_compare(mapping_of(n)->min, *(uint64_t*)key))

static inline void
mapping_add(Mapping* self, Slice* slice)
{
	RbtreeNode* node;
	int rc;
	rc = mapping_find(&self->tree, self->comparator, &slice->min, &node);
	if (rc == 0 && node)
		assert(0);
	rbtree_set(&self->tree, node, rc, &slice->node);
	self->tree_count++;
}

static inline void
mapping_remove(Mapping* self, Slice* slice)
{
	rbtree_remove(&self->tree, &slice->node);
	self->tree_count--;
	assert(self->tree_count >= 0);
}

static inline void
mapping_replace(Mapping* self, Slice* a, Slice* b)
{
	rbtree_replace(&self->tree, &a->node, &b->node);
}

static inline Slice*
mapping_next(Mapping* self, Slice* slice)
{
	auto node = rbtree_next(&self->tree, &slice->node);
	if (! node)
		return NULL;
	return mapping_of(node);
}

static inline Slice*
mapping_prev(Mapping* self, Slice* slice)
{
	auto node = rbtree_prev(&self->tree, &slice->node);
	if (! node)
		return NULL;
	return mapping_of(node);
}

hot static inline Slice*
mapping_seek(Mapping* self, uint64_t min)
{
	// slice[n].min >= key && key < slice[n + 1].min
	RbtreeNode* node = NULL;
	mapping_find(&self->tree, self->comparator, &min, &node);
	assert(node != NULL);
	return mapping_of(node);
}

hot static inline Slice*
mapping_gte(Mapping* self, uint64_t min)
{
	if (self->tree_count == 0)
		return NULL;
	auto slice = mapping_seek(self, min);
	if (slice->max <= min)
		return mapping_next(self, slice);
	return slice;
}

hot static inline Slice*
mapping_match(Mapping* self, uint64_t min)
{
	// exact match
	if (self->tree_count == 0)
		return NULL;
	auto slice = mapping_seek(self, min);
	if (slice_in(slice, min))
		return slice;
	return NULL;
}
