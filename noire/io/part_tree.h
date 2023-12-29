#pragma once

//
// noire
//
// time-series storage
//

typedef struct PartTree PartTree;

struct PartTree
{
	Rbtree      tree;
	int         tree_count;
	Comparator* comparator;
};

static inline void
part_tree_init(PartTree* self, Comparator* comparator)
{
	self->tree_count = 0;
	self->comparator = comparator;
	rbtree_init(&self->tree);
}

always_inline static inline Part*
part_tree_of(RbtreeNode* node)
{
	return container_of(node, Part, node);
}

static inline Part*
part_tree_min(PartTree* self)
{
	if (self->tree_count == 0)
		return NULL;
	auto min = rbtree_min(&self->tree);
	return container_of(min, Part, node);
}

static inline Part*
part_tree_max(PartTree* self)
{
	if (self->tree_count == 0)
		return NULL;
	auto max = rbtree_max(&self->tree);
	return container_of(max, Part, node);
}

always_inline static inline int
part_tree_compare(uint64_t a, uint64_t b)
{
	if (a == b)
		return 0;
	return (a > b) ? 1 : -1;
}

hot static inline
rbtree_get(part_tree_find, part_tree_compare(part_tree_of(n)->min, *(uint64_t*)key))

static inline void
part_tree_add(PartTree* self, Part* part)
{
	RbtreeNode* part_ptr;
	int rc;
	rc = part_tree_find(&self->tree, self->comparator, &part->min, &part_ptr);
	if (rc == 0 && part_ptr)
		assert(0);
	rbtree_set(&self->tree, part_ptr, rc, &part->node);
	self->tree_count++;
}

static inline void
part_tree_remove(PartTree* self, Part* part)
{
	rbtree_remove(&self->tree, &part->node);
	self->tree_count--;
	assert(self->tree_count >= 0);
}

static inline void
part_tree_replace(PartTree* self, Part* a, Part* b)
{
	rbtree_replace(&self->tree, &a->node, &b->node);
}

hot static inline Part*
part_tree_match(PartTree* self, uint64_t min)
{
	// part[n].min >= key && key < part[n + 1].min
	RbtreeNode* part_ptr = NULL;
	int rc;
	rc = part_tree_find(&self->tree, self->comparator, &min, &part_ptr);
	assert(part_ptr != NULL);
	if (rc == 1)
	{
		auto prev = rbtree_prev(&self->tree, part_ptr);
		if (prev)
			part_ptr = prev;
	}

	auto part = container_of(part_ptr, Part, node);
	if (part->min <= min && min < part->max)
		return part;

	return NULL;
}

static inline Part*
part_tree_next(PartTree* self, Part* part)
{
	auto node = rbtree_next(&self->tree, &part->node);
	if (! node)
		return NULL;
	return part_tree_of(node);
}

static inline Part*
part_tree_prev(PartTree* self, Part* part)
{
	auto node = rbtree_prev(&self->tree, &part->node);
	if (! node)
		return NULL;
	return part_tree_of(node);
}
