/*
 * Copyright (c) 2008, 2010, Oracle and/or its affiliates. All rights reserved.
 * Use is subject to license terms.
 */
/*
 * This file is part of Lustre, http://www.lustre.org/
 * Lustre is a trademark of Sun Microsystems, Inc.
 *
 * lustre/ldlm/interval_tree.c
 *
 * Interval tree library used by ldlm extent lock code
 *
 * Author: Huang Wei <huangwei@clusterfs.com>
 * Author: Jay Xiong <jinshan.xiong@sun.com>
 */

/*
 * For both kernel and userspace use
 * DO NOT use anything special that opposes this purpose
 */

#if defined(__linux__) && defined(__KERNEL__)
#include <linux/module.h>
#endif /* defined(__linux__) && defined(__KERNEL__) */
#include <debug.h>
#include <compat.h>
#include <mtfs_interval_tree.h>

enum {
	MTFS_INTERVAL_RED = 0,
	MTFS_INTERVAL_BLACK = 1
};

static inline int node_is_left_child(struct mtfs_interval_node *node)
{
	MASSERT(node->in_parent != NULL);
	return node == node->in_parent->in_left;
}

static inline int node_is_right_child(struct mtfs_interval_node *node)
{
	MASSERT(node->in_parent != NULL);
	return node == node->in_parent->in_right;
}

static inline int node_is_red(struct mtfs_interval_node *node)
{
	return node->in_color == MTFS_INTERVAL_RED;
}

static inline int node_is_black(struct mtfs_interval_node *node)
{
	return node->in_color == MTFS_INTERVAL_BLACK;
}

static inline int extent_compare(struct mtfs_interval_node_extent *e1,
                                 struct mtfs_interval_node_extent *e2)
{
	int rc;
	if (e1->start == e2->start) {
		if (e1->end < e2->end)
			rc = -1;
		else if (e1->end > e2->end)
			rc = 1;
		else
			rc = 0;
	} else {
		if (e1->start < e2->start)
			rc = -1;
		else
			rc = 1;
	}
	return rc;
}

static inline int extent_equal(struct mtfs_interval_node_extent *e1,
                               struct mtfs_interval_node_extent *e2)
{
	return (e1->start == e2->start) && (e1->end == e2->end);
}

static inline int extent_overlapped(struct mtfs_interval_node_extent *e1, 
                                    struct mtfs_interval_node_extent *e2)
{
	return (e1->start <= e2->end) && (e2->start <= e1->end);
}

static inline int node_compare(struct mtfs_interval_node *n1,
                               struct mtfs_interval_node *n2)
{
	return extent_compare(&n1->in_extent, &n2->in_extent);
}

static inline int node_equal(struct mtfs_interval_node *n1,
                             struct mtfs_interval_node *n2)
{
	return extent_equal(&n1->in_extent, &n2->in_extent);
}

static inline __u64 max_u64(__u64 x, __u64 y)
{
	return x > y ? x : y;
}

static inline __u64 min_u64(__u64 x, __u64 y)
{
	return x < y ? x : y;
}

#define mtfs_interval_for_each(node, root)                   \
for (node = mtfs_interval_first(root); node != NULL;         \
     node = mtfs_interval_next(node))

#define mtfs_interval_for_each_reverse(node, root)           \
for (node = mtfs_interval_last(root); node != NULL;          \
     node = mtfs_interval_prev(node))

static struct mtfs_interval_node *mtfs_interval_first(struct mtfs_interval_node *node)
{
	MENTRY();

	if (!node)
		MRETURN(NULL);
	while (node->in_left)
		node = node->in_left;
	MRETURN(node);
}

static struct mtfs_interval_node *mtfs_interval_last(struct mtfs_interval_node *node)
{
        MENTRY();

	if (!node)
		MRETURN(NULL);
	while (node->in_right)
		node = node->in_right;
        MRETURN(node);
}

static struct mtfs_interval_node *mtfs_interval_next(struct mtfs_interval_node *node)
{
	MENTRY();

	if (!node)
		MRETURN(NULL);
	if (node->in_right)
		MRETURN(mtfs_interval_first(node->in_right));
	while (node->in_parent && node_is_right_child(node))
		node = node->in_parent;
	MRETURN(node->in_parent);
}

static struct mtfs_interval_node *mtfs_interval_prev(struct mtfs_interval_node *node)
{
	MENTRY();

	if (!node)
		MRETURN(NULL);

	if (node->in_left)
		MRETURN(mtfs_interval_last(node->in_left));

	while (node->in_parent && node_is_left_child(node))
		node = node->in_parent;

	MRETURN(node->in_parent);
}

enum mtfs_interval_iter mtfs_interval_iterate(struct mtfs_interval_node *root,
                                    mtfs_interval_callback_t func,
                                    void *data)
{
	struct mtfs_interval_node *node;
	enum mtfs_interval_iter rc = MTFS_INTERVAL_ITER_CONT;
	MENTRY();

	mtfs_interval_for_each(node, root) {
		rc = func(node, data);
		if (rc == MTFS_INTERVAL_ITER_STOP)
			break;
	}

	MRETURN(rc);
}
EXPORT_SYMBOL(mtfs_interval_iterate);

enum mtfs_interval_iter mtfs_interval_iterate_reverse(struct mtfs_interval_node *root,
                                            mtfs_interval_callback_t func,
                                            void *data)
{
	struct mtfs_interval_node *node;
	enum mtfs_interval_iter rc = MTFS_INTERVAL_ITER_CONT;
	MENTRY();
        
	mtfs_interval_for_each_reverse(node, root) {
		rc = func(node, data);
		if (rc == MTFS_INTERVAL_ITER_STOP)
		break;
	}

	MRETURN(rc);
}
EXPORT_SYMBOL(mtfs_interval_iterate_reverse);

/* try to find a node with same mtfs_interval in the tree,
 * if found, return the pointer to the node, otherwise return NULL*/
struct mtfs_interval_node *mtfs_interval_find(struct mtfs_interval_node *root,
                                    struct mtfs_interval_node_extent *ex)
{
	struct mtfs_interval_node *walk = root;
	int rc;
	MENTRY();

	while (walk) {
		rc = extent_compare(ex, &walk->in_extent);
		if (rc == 0)
			break;
		else if (rc < 0)
  			walk = walk->in_left;
		else
			walk = walk->in_right;
        }

	MRETURN(walk);
}
EXPORT_SYMBOL(mtfs_interval_find);

static void __rotate_change_maxhigh(struct mtfs_interval_node *node,
                                    struct mtfs_interval_node *rotate)
{
	__u64 left_max, right_max;

	rotate->in_max_high = node->in_max_high;
	left_max = node->in_left ? node->in_left->in_max_high : 0;
	right_max = node->in_right ? node->in_right->in_max_high : 0;
	node->in_max_high  = max_u64(mtfs_interval_high(node),
	                             max_u64(left_max,right_max));
}

/* The left rotation "pivots" around the link from node to node->right, and
 * - node will be linked to node->right's left child, and
 * - node->right's left child will be linked to node's right child.  */
static void __rotate_left(struct mtfs_interval_node *node,
                          struct mtfs_interval_node **root)
{
	struct mtfs_interval_node *right = node->in_right;
	struct mtfs_interval_node *parent = node->in_parent;

	node->in_right = right->in_left;
	if (node->in_right)
		right->in_left->in_parent = node;

	right->in_left = node;
	right->in_parent = parent;
	if (parent) {
		if (node_is_left_child(node))
			parent->in_left = right;
		else
			parent->in_right = right;
        } else {
		*root = right;
	}
	node->in_parent = right;

        /* update max_high for node and right */
	__rotate_change_maxhigh(node, right);
}

/* The right rotation "pivots" around the link from node to node->left, and
 * - node will be linked to node->left's right child, and
 * - node->left's right child will be linked to node's left child.  */
static void __rotate_right(struct mtfs_interval_node *node,
                           struct mtfs_interval_node **root)
{
	struct mtfs_interval_node *left = node->in_left;
	struct mtfs_interval_node *parent = node->in_parent;

	node->in_left = left->in_right;
	if (node->in_left)
		left->in_right->in_parent = node;
	left->in_right = node;

	left->in_parent = parent;
	if (parent) {
		if (node_is_right_child(node))
			parent->in_right = left;
		else
			parent->in_left = left;
	} else {
			*root = left;
	}
	node->in_parent = left;

	/* update max_high for node and left */
	__rotate_change_maxhigh(node, left);
}

#define mtfs_interval_swap(a, b) do {                        \
        struct mtfs_interval_node *c = a; a = b; b = c;      \
} while (0)

/*
 * Operations INSERT and DELETE, when run on a tree with n keys, 
 * take O(logN) time.Because they modify the tree, the result 
 * may violate the red-black properties.To restore these properties, 
 * we must change the colors of some of the nodes in the tree 
 * and also change the pointer structure.
 */
static void mtfs_interval_insert_color(struct mtfs_interval_node *node,
                                  struct mtfs_interval_node **root)
{
	struct mtfs_interval_node *parent, *gparent;
	MENTRY();

	while ((parent = node->in_parent) && node_is_red(parent)) {
		gparent = parent->in_parent;
		/* Parent is RED, so gparent must not be NULL */
		if (node_is_left_child(parent)) {
			struct mtfs_interval_node *uncle;
			uncle = gparent->in_right;
			if (uncle && node_is_red(uncle)) {
				uncle->in_color = MTFS_INTERVAL_BLACK;
				parent->in_color = MTFS_INTERVAL_BLACK;
				gparent->in_color = MTFS_INTERVAL_RED;
				node = gparent;
				continue;
			}

			if (parent->in_right == node) {
				__rotate_left(parent, root);
				mtfs_interval_swap(node, parent);
			}

			parent->in_color = MTFS_INTERVAL_BLACK;
			gparent->in_color = MTFS_INTERVAL_RED;
			__rotate_right(gparent, root);
		} else {
			struct mtfs_interval_node *uncle;
			uncle = gparent->in_left;
			if (uncle && node_is_red(uncle)) {
				uncle->in_color = MTFS_INTERVAL_BLACK;
				parent->in_color = MTFS_INTERVAL_BLACK;
				gparent->in_color = MTFS_INTERVAL_RED;
				node = gparent;
				continue;
			}

			if (node_is_left_child(node)) {
				__rotate_right(parent, root);
				mtfs_interval_swap(node, parent);
			}

			parent->in_color = MTFS_INTERVAL_BLACK;
			gparent->in_color = MTFS_INTERVAL_RED;
			__rotate_left(gparent, root);
		}
	}

	(*root)->in_color = MTFS_INTERVAL_BLACK;
	_MRETURN();
}

struct mtfs_interval_node *mtfs_interval_insert(struct mtfs_interval_node *node,
                                      struct mtfs_interval_node **root)
                     
{
	struct mtfs_interval_node **p, *parent = NULL;
        MENTRY();

	MASSERT(!mtfs_interval_is_intree(node));
	p = root;
	while (*p) {
		parent = *p;
		if (node_equal(parent, node))
			MRETURN(parent);

		/* max_high field must be updated after each iteration */
		if (parent->in_max_high < mtfs_interval_high(node))
			parent->in_max_high = mtfs_interval_high(node);

		if (node_compare(node, parent) < 0)
			p = &parent->in_left;
		else 
			p = &parent->in_right;
	}

	/* link node into the tree */
	node->in_parent = parent;
	node->in_color = MTFS_INTERVAL_RED;
	node->in_left = node->in_right = NULL;
	*p = node;

	mtfs_interval_insert_color(node, root);
	node->in_intree = 1;

	MRETURN(NULL);
}
EXPORT_SYMBOL(mtfs_interval_insert);

static inline int node_is_black_or_0(struct mtfs_interval_node *node)
{
	return !node || node_is_black(node);
}

static void mtfs_interval_erase_color(struct mtfs_interval_node *node,
                                 struct mtfs_interval_node *parent,
                                 struct mtfs_interval_node **root)
{
	struct mtfs_interval_node *tmp;
	MENTRY();

	while (node_is_black_or_0(node) && node != *root) {
		if (parent->in_left == node) {
        		tmp = parent->in_right;
        		if (node_is_red(tmp)) {
        			tmp->in_color = MTFS_INTERVAL_BLACK;
        			parent->in_color = MTFS_INTERVAL_RED;
        			__rotate_left(parent, root);
        			tmp = parent->in_right;
        		}
        		if (node_is_black_or_0(tmp->in_left) &&
        		    node_is_black_or_0(tmp->in_right)) {
        			tmp->in_color = MTFS_INTERVAL_RED;
        			node = parent;
        			parent = node->in_parent;
        		} else {
        			if (node_is_black_or_0(tmp->in_right)) {
        				struct mtfs_interval_node *o_left;
        				if ((o_left = tmp->in_left))
        					o_left->in_color = MTFS_INTERVAL_BLACK;
        				tmp->in_color = MTFS_INTERVAL_RED;
        				__rotate_right(tmp, root);
        				tmp = parent->in_right;
        			}
        			tmp->in_color = parent->in_color;
        			parent->in_color = MTFS_INTERVAL_BLACK;
        			if (tmp->in_right)
        				tmp->in_right->in_color = MTFS_INTERVAL_BLACK;
        			__rotate_left(parent, root);
        			node = *root;
        			break;
        		}
		} else {
        		tmp = parent->in_left;
        		if (node_is_red(tmp)) {
        			tmp->in_color = MTFS_INTERVAL_BLACK;
				parent->in_color = MTFS_INTERVAL_RED;
        			__rotate_right(parent, root);
        			tmp = parent->in_left;
        		}
        		if (node_is_black_or_0(tmp->in_left) &&
        		    node_is_black_or_0(tmp->in_right)) {
        			tmp->in_color = MTFS_INTERVAL_RED;
        			node = parent;
        			parent = node->in_parent;
        		} else {
        			if (node_is_black_or_0(tmp->in_left)) {
        				struct mtfs_interval_node *o_right;
        				if ((o_right = tmp->in_right))
        					o_right->in_color = MTFS_INTERVAL_BLACK;
        				tmp->in_color = MTFS_INTERVAL_RED;
        				__rotate_left(tmp, root);
        				tmp = parent->in_left;
        			}
        			tmp->in_color = parent->in_color;
        			parent->in_color = MTFS_INTERVAL_BLACK;
        			if (tmp->in_left)
        				tmp->in_left->in_color = MTFS_INTERVAL_BLACK;
        			__rotate_right(parent, root);
        			node = *root;
        			break;
        		}
		}
	}
	if (node)
		node->in_color = MTFS_INTERVAL_BLACK;
	_MRETURN();
}

/* 
 * if the @max_high value of @node is changed, this function traverse  a path 
 * from node  up to the root to update max_high for the whole tree.
 */
static void update_maxhigh(struct mtfs_interval_node *node,
                           __u64  old_maxhigh)
{
	__u64 left_max, right_max;
	MENTRY();

	while (node) {
		left_max = node->in_left ? node->in_left->in_max_high : 0;
		right_max = node->in_right ? node->in_right->in_max_high : 0;
		node->in_max_high = max_u64(mtfs_interval_high(node),
		                    max_u64(left_max, right_max));

		if (node->in_max_high >= old_maxhigh)
			break;
		node = node->in_parent;
	}
	_MRETURN();
}

void mtfs_interval_erase(struct mtfs_interval_node *node,
                    struct mtfs_interval_node **root)
{
	struct mtfs_interval_node *child, *parent;
	int color;
	MENTRY();

	MASSERT(mtfs_interval_is_intree(node));
	node->in_intree = 0;
	if (!node->in_left) {
		child = node->in_right;
	} else if (!node->in_right) {
		child = node->in_left;
	} else { /* Both left and right child are not NULL */
		struct mtfs_interval_node *old = node;

		node = mtfs_interval_next(node);
		child = node->in_right;
		parent = node->in_parent;
		color = node->in_color;

		if (child)
			child->in_parent = parent;
		if (parent == old)
			parent->in_right = child;
		else
			parent->in_left = child;

		node->in_color = old->in_color;
		node->in_right = old->in_right;
		node->in_left = old->in_left;
		node->in_parent = old->in_parent;

		if (old->in_parent) {
			if (node_is_left_child(old))
				old->in_parent->in_left = node;
			else
				old->in_parent->in_right = node;
		} else {
			*root = node;
		}

		old->in_left->in_parent = node;
		if (old->in_right)
			old->in_right->in_parent = node;
		update_maxhigh(child ? : parent, node->in_max_high);
		update_maxhigh(node, old->in_max_high);
		if (parent == old)
			parent = node;
		goto color;
	}
	parent = node->in_parent;
	color = node->in_color;

	if (child)
		child->in_parent = parent;
	if (parent) {
		if (node_is_left_child(node))
			parent->in_left = child;
		else
			parent->in_right = child;
	} else {
		*root = child;
	}

	update_maxhigh(child ? : parent, node->in_max_high);

color:
	if (color == MTFS_INTERVAL_BLACK)
		mtfs_interval_erase_color(child, parent, root);
	_MRETURN();
}
EXPORT_SYMBOL(mtfs_interval_erase);

static inline int mtfs_interval_may_overlap(struct mtfs_interval_node *node,
                                          struct mtfs_interval_node_extent *ext)
{
	return (ext->start <= node->in_max_high &&
		ext->end >= mtfs_interval_low(node));
}

/*
 * This function finds all mtfs_intervals that overlap mtfs_interval ext,
 * and calls func to handle resulted mtfs_intervals one by one.
 * in lustre, this function will find all conflicting locks in
 * the granted queue and add these locks to the ast work list.
 *
 * {
 *       if (node == NULL)
 *               return 0;
 *       if (ext->end < mtfs_interval_low(node)) {
 *               mtfs_interval_search(node->in_left, ext, func, data);
 *       } else if (mtfs_interval_may_overlap(node, ext)) {
 *               if (extent_overlapped(ext, &node->in_extent))
 *                       func(node, data);
 *               mtfs_interval_search(node->in_left, ext, func, data);
 *               mtfs_interval_search(node->in_right, ext, func, data);
 *       }
 *       return 0;
 * }
 *
 */
enum mtfs_interval_iter mtfs_interval_search(struct mtfs_interval_node *node,
                                   struct mtfs_interval_node_extent *ext,
                                   mtfs_interval_callback_t func,
                                   void *data)
{
	struct mtfs_interval_node *parent;
	enum mtfs_interval_iter rc = MTFS_INTERVAL_ITER_CONT;

	MASSERT(ext != NULL);
	MASSERT(func != NULL);

	while (node) {
		if (ext->end < mtfs_interval_low(node)) {
			if (node->in_left) {
				node = node->in_left;
				continue;
			}
		} else if (mtfs_interval_may_overlap(node, ext)) {
			if (extent_overlapped(ext, &node->in_extent)) {
				rc = func(node, data);
				if (rc == MTFS_INTERVAL_ITER_STOP)
					break;
			}

			if (node->in_left) {
				node = node->in_left;
				continue;
			}
			if (node->in_right) {
				node = node->in_right;
				continue;
			}
		} 

		parent = node->in_parent;
		while (parent) {
			if (node_is_left_child(node) &&
			    parent->in_right) {
				/* If we ever got the left, it means that the 
				 * parent met ext->end<mtfs_interval_low(parent), or
				 * may_overlap(parent). If the former is true,
				 * we needn't go back. So stop early and check
				 * may_overlap(parent) after this loop.  */
				node = parent->in_right;
				break;
			}
			node = parent;
			parent = parent->in_parent;
		}
		if (parent == NULL || !mtfs_interval_may_overlap(parent, ext))
			break;
	}

	return rc;
}
EXPORT_SYMBOL(mtfs_interval_search);

static enum mtfs_interval_iter mtfs_interval_overlap_cb(struct mtfs_interval_node *n,
                                              void *args)
{
	*(int *)args = 1;
	return MTFS_INTERVAL_ITER_STOP;
}

int mtfs_interval_is_overlapped(struct mtfs_interval_node *root,
                           struct mtfs_interval_node_extent *ext)
{
	int has = 0;
	(void)mtfs_interval_search(root, ext, mtfs_interval_overlap_cb, &has);
	return has;
}
EXPORT_SYMBOL(mtfs_interval_is_overlapped);

/* Don't expand to low. Expanding downwards is expensive, and meaningless to
 * some extents, because programs seldom do IO backward.
 *
 * The recursive algorithm of expanding low:
 * expand_low {
 *        struct mtfs_interval_node *tmp;
 *        static __u64 res = 0;
 *
 *        if (root == NULL)
 *                return res;
 *        if (root->in_max_high < low) {
 *                res = max_u64(root->in_max_high + 1, res);
 *                return res;
 *        } else if (low < mtfs_interval_low(root)) {
 *                mtfs_interval_expand_low(root->in_left, low);
 *                return res;
 *        }
 *
 *        if (mtfs_interval_high(root) < low)
 *                res = max_u64(mtfs_interval_high(root) + 1, res);
 *        mtfs_interval_expand_low(root->in_left, low);
 *        mtfs_interval_expand_low(root->in_right, low);
 *
 *        return res;
 * }
 *
 * It's much easy to eliminate the recursion, see mtfs_interval_search for 
 * an example. -jay
 */
static inline __u64 mtfs_interval_expand_low(struct mtfs_interval_node *root, __u64 low)
{
	/* we only concern the empty tree right now. */
	if (root == NULL)
		return 0;
	return low;
}

static inline __u64 mtfs_interval_expand_high(struct mtfs_interval_node *node, __u64 high)
{
	__u64 result = ~0;

	while (node != NULL) {
		if (node->in_max_high < high)
			break;

		if (mtfs_interval_low(node) > high) {
			result = mtfs_interval_low(node) - 1;
			node = node->in_left;
		} else {
			node = node->in_right;
		}
	}

	return result;
}

/* expanding the extent based on @ext. */
void mtfs_interval_expand(struct mtfs_interval_node *root,
                     struct mtfs_interval_node_extent *ext,
                     struct mtfs_interval_node_extent *limiter)
{
	/* The assertion of mtfs_interval_is_overlapped is expensive because we may
	 * travel many nodes to find the overlapped node. */
	MASSERT(mtfs_interval_is_overlapped(root, ext) == 0);
	if (!limiter || limiter->start < ext->start)
		ext->start = mtfs_interval_expand_low(root, ext->start);
	if (!limiter || limiter->end > ext->end)
		ext->end = mtfs_interval_expand_high(root, ext->end);
	MASSERT(mtfs_interval_is_overlapped(root, ext) == 0);
}
EXPORT_SYMBOL(mtfs_interval_expand);
