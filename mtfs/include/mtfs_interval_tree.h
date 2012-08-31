/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef _MTFS_INTERVAL_TREE_H_
#define _MTFS_INTERVAL_TREE_H_

#include <asm/types.h>
#include <debug.h>

struct mtfs_interval_node {
	struct mtfs_interval_node   *in_left;
	struct mtfs_interval_node   *in_right;
	struct mtfs_interval_node   *in_parent;
	unsigned                     in_color:1,
                                     in_intree:1, /** set if the node is in tree */
                                     in_res1:30;
	__u8                         in_res2[4];  /** tags, 8-bytes aligned */
	__u64                        in_max_high;
	struct mtfs_interval_node_extent {
		__u64 start;
		__u64 end;
	} in_extent;
};

enum mtfs_interval_iter {
	MTFS_INTERVAL_ITER_CONT = 1,
	MTFS_INTERVAL_ITER_STOP = 2
};

static inline int mtfs_interval_is_intree(struct mtfs_interval_node *node)
{
	return node->in_intree == 1;
}

static inline __u64 mtfs_interval_low(struct mtfs_interval_node *node)
{
	return node->in_extent.start;
}

static inline __u64 mtfs_interval_high(struct mtfs_interval_node *node)
{
	return node->in_extent.end;
}

static inline void mtfs_interval_set(struct mtfs_interval_node *node,
                                     __u64 start, __u64 end)
{
	HASSERT(start <= end);
	node->in_extent.start = start;
	node->in_extent.end = end;
	node->in_max_high = end;
}

/* Rules to write an mtfs_interval callback.
 *  - the callback returns MTFS_INTERVAL_ITER_STOP when it thinks the iteration
 *    should be stopped. It will then cause the iteration function to return
 *    immediately with return value MTFS_INTERVAL_ITER_STOP.
 *  - callbacks for mtfs_interval_iterate and mtfs_interval_iterate_reverse: Every 
 *    nodes in the tree will be set to @node before the callback being called
 *  - callback for mtfs_interval_search: Only overlapped node will be set to @node
 *    before the callback being called.
 */
typedef enum mtfs_interval_iter (*mtfs_interval_callback_t)(struct mtfs_interval_node *node,
                                                            void *args);

struct mtfs_interval_node *mtfs_interval_insert(struct mtfs_interval_node *node,
                                                struct mtfs_interval_node **root);
void mtfs_interval_erase(struct mtfs_interval_node *node, struct mtfs_interval_node **root);

/* Search the extents in the tree and call @func for each overlapped
 * extents. */
enum mtfs_interval_iter mtfs_interval_search(struct mtfs_interval_node *root,
                                             struct mtfs_interval_node_extent *ex,
                                             mtfs_interval_callback_t func, void *data);

/* Iterate every node in the tree - by reverse order or regular order. */
enum mtfs_interval_iter mtfs_interval_iterate(struct mtfs_interval_node *root, 
                                              mtfs_interval_callback_t func, void *data);
enum mtfs_interval_iter mtfs_interval_iterate_reverse(struct mtfs_interval_node *root,
                                                      mtfs_interval_callback_t func,void *data);

void mtfs_interval_expand(struct mtfs_interval_node *root, 
                          struct mtfs_interval_node_extent *ext,
                          struct mtfs_interval_node_extent *limiter);
int mtfs_interval_is_overlapped(struct mtfs_interval_node *root, 
                           struct mtfs_interval_node_extent *ex);
struct mtfs_interval_node *mtfs_interval_find(struct mtfs_interval_node *root,
                                              struct mtfs_interval_node_extent *ex);
#endif /* _MTFS_INTERVAL_TREE_H_ */
