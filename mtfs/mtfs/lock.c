/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <linux/sched.h>
#include <debug.h>
#include <memory.h>
#include <thread.h>
#include <mtfs_lock.h>
#include <mtfs_list.h>
#include <mtfs_interval_tree.h>
#include <mtfs_inode.h>
#include "main_internal.h"

/* compatibility matrix */
mlock_mode_t mlock_compat_array[] = {
        [MLOCK_MODE_READ]  MLOCK_COMPAT_READ,
        [MLOCK_MODE_WRITE] MLOCK_COMPAT_WRITE,
        [MLOCK_MODE_NULL]  MLOCK_COMPAT_NULL,
};

struct mlock_plain_position {
	mtfs_list_t *res_link;
	mtfs_list_t *mode_link;
};

static void mlock_plain_search_prev(mtfs_list_t *queue, struct mlock *req_lock, struct mlock_plain_position *prev)
{
	mtfs_list_t *tmp = NULL;
	struct mlock *tmp_lock = NULL;
	struct mlock *lock_mode_end = NULL;
	HENTRY();

	mtfs_list_for_each(tmp, queue) {
		tmp_lock = mtfs_list_entry(tmp, struct mlock, ml_res_link);
		lock_mode_end = mtfs_list_entry(tmp_lock->ml_mode_link.prev,
                                                struct mlock, ml_mode_link);

		if (tmp_lock->ml_mode != req_lock->ml_mode) {
			/* jump to last lock of mode group */
			tmp = &lock_mode_end->ml_res_link;
			continue;
		}

		/* Suitable mode group is found */

		/* Insert point is last lock of the mode group */
		prev->res_link = &lock_mode_end->ml_res_link;
		prev->mode_link = &lock_mode_end->ml_mode_link;
		break;
	}

	/*
	 * Insert point is last lock on the queue,
         * new mode group and new policy group are started
         */
	prev->res_link = queue->prev;
	prev->mode_link = &req_lock->ml_mode_link;

	_HRETURN();
}

static void mlock_plain_unlink(struct mlock *lock)
{
	HENTRY();

	HASSERT(mlock_resource_is_locked(lock->ml_resource));
	mtfs_list_del_init(&lock->ml_mode_link);
	_HRETURN();
}

static void mlock_plain_add(struct mlock *lock, struct mlock_plain_position *prev)
{
	HENTRY();

	HASSERT(mlock_resource_is_locked(lock->ml_resource));
	HASSERT(lock->ml_state == MLOCK_STATE_GRANTED);
        HASSERT(mtfs_list_empty(&lock->ml_res_link));

        mtfs_list_add(&lock->ml_res_link, prev->res_link);
        mtfs_list_add(&lock->ml_mode_link, prev->mode_link);	
	_HRETURN();
}

/*
 * 0 if the lock is compatible
 * 1 if the lock is not compatible
 */
static int mlock_plain_conflict(mtfs_list_t *queue, struct mlock *req_lock)
{
	int ret = 0;
	struct mlock_resource *resource = req_lock->ml_resource;
	struct mlock *old_lock = NULL;
	mtfs_list_t *tmp = NULL;
	HENTRY();

	HASSERT(mlock_resource_is_locked(resource));
	HASSERT(queue == &resource->mlr_granted || queue == &resource->mlr_waiting);

	mtfs_list_for_each(tmp, queue) {
		old_lock = mtfs_list_entry(tmp, struct mlock, ml_res_link);

		if (req_lock == old_lock) {
			HASSERT(queue == &resource->mlr_waiting);
			break;
		}

		/* last lock in mode group */
		tmp = &mtfs_list_entry(old_lock->ml_mode_link.prev,
                                       struct mlock,
                                       ml_mode_link)->ml_res_link;

		if (mlock_mode_compat(old_lock->ml_mode, req_lock->ml_mode)) {
			continue;
		}

		ret = 1;
		break;
	}

	HRETURN(ret);
}

static int mlock_pain_init(struct mlock *lock, struct mlock_enqueue_info *einfo)
{
	int ret = 0;
	HENTRY();
	HASSERT(lock->ml_type->mto_type == MLOCK_TYPE_PLAIN);

	MTFS_INIT_LIST_HEAD(&lock->ml_mode_link);

	HRETURN(ret);
}

static void mlock_plain_grant(struct mlock *lock)
{
	struct mlock_resource *resource = lock->ml_resource;
	struct mlock_plain_position prev;
	HENTRY();

	HASSERT(mlock_resource_is_locked(resource));
	HASSERT(mtfs_list_empty(&lock->ml_mode_link));
	HASSERT(mtfs_list_empty(&lock->ml_res_link));

	mlock_plain_search_prev(&resource->mlr_granted, lock, &prev);
	mlock_plain_add(lock, &prev);
	_HRETURN();
}

struct mlock_type_object mlock_type_pain = {
	.mto_type = MLOCK_TYPE_PLAIN,
	.mto_init = mlock_pain_init,
	.mto_grant = mlock_plain_grant,
	.mto_conflict = mlock_plain_conflict,
	.mto_unlink = mlock_plain_unlink,
};

static inline int __is_po2(unsigned long long val)
{
        return !(val & (val - 1));
}

#define IS_PO2(val) __is_po2((unsigned long long)(val))

static inline int lock_mode_to_index(mlock_mode_t mode)
{
	int index = 0;

	HASSERT(mode != 0);
	HASSERT(IS_PO2(mode));
	for (index = -1; mode; index++, mode >>= 1) ;
	HASSERT(index < MLOCK_MODE_MAX);
	return index;
}

void mlock_interval_attach(struct mlock_interval *node,
                           struct mlock *lock)
{
	HASSERT(lock->ml_tree_node == NULL);
	HASSERT(lock->ml_type->mto_type == MLOCK_TYPE_EXTENT);
	HENTRY();

	mtfs_list_add_tail(&lock->ml_policy_link, &node->mli_group);
	lock->ml_tree_node = node;
	_HRETURN();
}

struct mlock_interval *mlock_interval_detach(struct mlock *lock)
{
        struct mlock_interval *node = lock->ml_tree_node;
	HENTRY();

	HASSERT(node);
	HASSERT(!mtfs_list_empty(&node->mli_group));
	lock->ml_tree_node = NULL;
	mtfs_list_del_init(&lock->ml_policy_link);

        return (mtfs_list_empty(&node->mli_group) ? node : NULL);
}
static int mlock_extent_init(struct mlock *lock, struct mlock_enqueue_info *einfo)
{
	struct mlock_interval *node = NULL;
	int ret = 0;
	HENTRY();

	HASSERT(lock->ml_type->mto_type == MLOCK_TYPE_EXTENT);
	MTFS_SLAB_ALLOC_PTR(node, mtfs_interval_cache);
	if (node == NULL) {
		HERROR("failed to create interval, not enough memory\n");
		ret = -ENOMEM;
		goto out;
	}

	//lock->ml_policy_data.mlp_extent = einfo->data.mlp_extent;
	lock->ml_policy_data.mlp_extent.start = 0;
	lock->ml_policy_data.mlp_extent.end = MLOCK_EXTENT_EOF;

	MTFS_INIT_LIST_HEAD(&node->mli_group);
	mlock_interval_attach(node, lock);
out:
	HRETURN(ret);
}

static void mlock_interval_free(struct mlock_interval *node)
{
	HASSERT(mtfs_list_empty(&node->mli_group));
	HASSERT(!mtfs_interval_is_intree(&node->mli_node));
	MTFS_SLAB_FREE_PTR(node, mtfs_interval_cache);
}

static void mlock_extent_grant(struct mlock *lock)
{
	struct mtfs_interval_node *found = NULL;
	struct mtfs_interval_node **root = NULL;
	struct mlock_interval *node = NULL;
	struct mlock_extent *extent = NULL;
	struct mlock_resource *resource = lock->ml_resource;
	int index = 0;
	HENTRY();

	HASSERT(mlock_resource_is_locked(resource));

	node = lock->ml_tree_node;
	HASSERT(node);
	HASSERT(!mtfs_interval_is_intree(&node->mli_node));

	index = lock_mode_to_index(lock->ml_mode);
	HASSERT(lock->ml_mode == 1 << index);
	HASSERT(lock->ml_mode == resource->mlr_itree[index].mlit_mode);

	extent = &lock->ml_policy_data.mlp_extent;
	mtfs_interval_set(&node->mli_node, extent->start, extent->end);

	root = &resource->mlr_itree[index].mlit_root;
	found = mtfs_interval_insert(&node->mli_node, root);
        if (found) {
        	/* The policy group found */
		struct mlock_interval *tmp = mlock_interval_detach(lock);
		HASSERT(tmp != NULL);
		mlock_interval_free(tmp);
		mlock_interval_attach(node2mlock_interval(found), lock);
        }
        resource->mlr_itree[index].mlit_size++;

	_HRETURN();
}

void mlock_extent_unlink(struct mlock *lock)
{
	struct mlock_resource *resource = lock->ml_resource;
	struct mlock_interval *node = lock->ml_tree_node;
	struct mlock_interval_tree *tree = NULL;
	int index = 0;
	HENTRY();

	HASSERT(node);
	HASSERT(mtfs_interval_is_intree(&node->mli_node));

	index = lock_mode_to_index(lock->ml_mode);
	HASSERT(lock->ml_mode == 1 << index);
	tree = &resource->mlr_itree[index];
	mtfs_list_del_init(&lock->ml_res_link);
	HASSERT(tree->mlit_root);
	tree->mlit_size--;
	node = mlock_interval_detach(lock);
	if (node) {
		mtfs_interval_erase(&node->mli_node, &tree->mlit_root);
		mlock_interval_free(node);
	}

	_HRETURN();
}

static int mlock_extent_conflict_granted(struct mlock *lock)
{
	int ret = 0;
	struct mlock_interval_tree *tree = NULL;
	struct mtfs_interval_node_extent extent;
	int index = 0;
	struct mlock_resource *resource = lock->ml_resource;
	HENTRY();

	HASSERT(mlock_resource_is_locked(resource));

	extent.start = lock->ml_policy_data.mlp_extent.start,
	extent.end = lock->ml_policy_data.mlp_extent.end;

	for (index = 0; index < MLOCK_MODE_NUM; index++) {
		tree = &resource->mlr_itree[index];
		if (mlock_mode_compat(tree->mlit_mode, lock->ml_mode)) {
			continue;
		}

		ret = mtfs_interval_is_overlapped(tree->mlit_root,&extent);
		if (ret) {
			break;
		}
	}

	HRETURN(ret);
}

static int mlock_extent_conflict_waiting(struct mlock *req_lock)
{
	int ret = 0;
	struct mlock_resource *resource = req_lock->ml_resource;
	struct mlock *old_lock = NULL;
	mtfs_list_t *tmp = NULL;
	mtfs_list_t *queue = &resource->mlr_waiting;
	__u64 old_start = 0;
	__u64 old_end = 0;
	__u64 req_start = 0;
	__u64 req_end = 0;
	HENTRY();

	mtfs_list_for_each(tmp, queue) {
		old_lock = mtfs_list_entry(tmp, struct mlock, ml_res_link);

		if (req_lock == old_lock) {
			break;
		}

		/* last lock in mode group */
		tmp = &mtfs_list_entry(old_lock->ml_mode_link.prev,
                                       struct mlock,
                                       ml_mode_link)->ml_res_link;

		if (mlock_mode_compat(old_lock->ml_mode, req_lock->ml_mode)) {
			continue;
		}

		req_start = req_lock->ml_policy_data.mlp_extent.start;
		req_end = req_lock->ml_policy_data.mlp_extent.end;
		old_start = old_lock->ml_policy_data.mlp_extent.start;
		old_end = old_lock->ml_policy_data.mlp_extent.end;
		if (old_end < req_start || old_start > req_end) {
			/* Doesn't overlap skip it */
			continue;
		}
		ret = 1;
		break;
	}

	HRETURN(ret);
}

/*
 * 0 if the lock is compatible
 * 1 if the lock is not compatible
 */
static int mlock_extent_conflict(mtfs_list_t *queue, struct mlock *lock)
{
	int ret = 0;
	struct mlock_resource *resource = lock->ml_resource;
	HENTRY();

	HASSERT(mlock_resource_is_locked(resource));
	HASSERT(queue == &resource->mlr_granted || queue == &resource->mlr_waiting);

	if (queue == &resource->mlr_granted) {
		ret = mlock_extent_conflict_granted(lock);
	} else {
		ret = mlock_extent_conflict_waiting(lock);
	}

	HRETURN(ret);
}

struct mlock_type_object mlock_type_extent = {
	.mto_type = MLOCK_TYPE_EXTENT,
	.mto_init = mlock_extent_init,
	.mto_grant = mlock_extent_grant,
	.mto_conflict = mlock_extent_conflict,
	.mto_unlink = mlock_extent_unlink,
};


static struct mlock *mlock_create(struct mlock_resource *resource, struct mlock_enqueue_info *einfo)
{
	struct mlock *lock = NULL;
	int ret = 0;
	HENTRY();

	MTFS_SLAB_ALLOC_PTR(lock, mtfs_lock_cache);
	if (lock == NULL) {
		HERROR("failed to create lock, not enough memory\n");
		goto out;
	}

	lock->ml_resource = resource;
	lock->ml_mode = einfo->mode;
	lock->ml_type = resource->mlr_type;
	MTFS_INIT_LIST_HEAD(&lock->ml_res_link);
	lock->ml_state = MLOCK_STATE_NEW;
	init_waitqueue_head(&lock->ml_waitq);
	lock->ml_pid = current->pid;

	if (lock->ml_type->mto_init) {
		ret = lock->ml_type->mto_init(lock, einfo);
		if (ret) {
			goto out_free_lock;
		}
	}
	goto out;
out_free_lock:
	MTFS_SLAB_FREE_PTR(lock, mtfs_lock_cache);
	lock = NULL;
out:
	HRETURN(lock);
}

static void mlock_grant(struct mlock *lock)
{
	struct mlock_resource *resource = lock->ml_resource;
	HENTRY();

	HASSERT(mlock_resource_is_locked(resource));
	HASSERT(lock->ml_state == MLOCK_STATE_NEW || lock->ml_state == MLOCK_STATE_WAITING);

	lock->ml_state = MLOCK_STATE_GRANTED;
	mtfs_list_del_init(&lock->ml_res_link);
	if (lock->ml_type->mto_grant) {
		lock->ml_type->mto_grant(lock);
	}

	if (mtfs_list_empty(&lock->ml_res_link)) {
		/* Some type may insert to the queue by itself*/
		mtfs_list_add(&lock->ml_res_link, &resource->mlr_granted);
	}
	_HRETURN();
}

static void mlock_unlink(struct mlock *lock)
{
	struct mlock_resource *resource = lock->ml_resource;
	HENTRY();

	HASSERT(mlock_resource_is_locked(resource));
	if (lock->ml_type->mto_unlink) {
		lock->ml_type->mto_unlink(lock);
	}
	mtfs_list_del_init(&lock->ml_res_link);
	_HRETURN();
}

/*
 * 0 if the lock is compatible
 * 1 if the lock is not compatible
 */
static int mlock_confilct(mtfs_list_t *queue, struct mlock *lock)
{
	int ret = 0;
	struct mlock_resource *resource = lock->ml_resource;
	HENTRY();

	HASSERT(mlock_resource_is_locked(resource));
	HASSERT(queue == &resource->mlr_granted || queue == &resource->mlr_waiting);

	if (lock->ml_type->mto_conflict) {
		ret = lock->ml_type->mto_conflict(queue, lock);
	}

	HRETURN(ret);
}

static void mlock_pend(struct mlock *lock)
{
	struct mlock_resource *resource = lock->ml_resource;
	HENTRY();

	HASSERT(mlock_resource_is_locked(resource));
	HASSERT(mtfs_list_empty(&lock->ml_res_link));
	HASSERT(lock->ml_state == MLOCK_STATE_NEW);

        mtfs_list_add_tail(&lock->ml_res_link, &resource->mlr_waiting);
        lock->ml_state = MLOCK_STATE_WAITING;

	_HRETURN();
}

static int mlock_enqueue_try_nolock(struct mlock *lock)
{
	struct mlock_resource *resource = lock->ml_resource;
	int ret = 0;
	HENTRY();

	HASSERT(mlock_resource_is_locked(resource));
	HASSERT(lock->ml_mode > MLOCK_MODE_MIN && lock->ml_mode < MLOCK_MODE_MAX);

	ret = mlock_confilct(&resource->mlr_granted, lock);
	if (ret) {
		HDEBUG("lock %p conflicting with granted queue\n", lock);
		goto out_pend;
	}

	ret = mlock_confilct(&resource->mlr_waiting, lock);
	if (ret) {
		HDEBUG("lock %p conflicting with waiting queue\n", lock);
		goto out_pend;
	}

	mlock_grant(lock);
	goto out;
out_pend:
	if (lock->ml_state == MLOCK_STATE_NEW) {
		mlock_pend(lock);
	}
out:
	HRETURN(ret);
}

static int mlock_enqueue_try(struct mlock *lock)
{
	struct mlock_resource *resource = lock->ml_resource;
	int ret = 0;
	HENTRY();

	mlock_resource_lock(resource);
	ret = mlock_enqueue_try_nolock(lock);
	mlock_resource_unlock(resource);
	HRETURN(ret);
}

struct mlock *mlock_enqueue(struct inode *inode, struct mlock_enqueue_info *einfo)
{
	struct mlock_resource *resource = mtfs_i2resource(inode);
	int ret = 0;
	struct mlock *lock = NULL;
	HENTRY();

	lock = mlock_create(resource, einfo);
	if (lock == NULL) {
		HERROR("failed to create lock\n");
	}

	ret = mlock_enqueue_try(lock);
	if (ret) {
		mtfs_wait_condition(lock->ml_waitq, mlock_is_granted(lock));
		HDEBUG("lock is granted because another lock is canceled\n");
	}
	HRETURN(lock);
}

void mlock_destroy(struct mlock *lock)
{
	struct mlock_resource *resource = lock->ml_resource;
	HENTRY();

	HASSERT(mlock_resource_is_locked(resource));
	HASSERT(mtfs_list_empty(&lock->ml_res_link));

	MTFS_SLAB_FREE_PTR(lock, mtfs_lock_cache);

	_HRETURN();
}

void mlock_resource_reprocess(struct mlock_resource *resource)
{
	mtfs_list_t *tmp = NULL;
	mtfs_list_t *pos = NULL;
	struct mlock *lock = NULL;
	int ret = 0;
	HENTRY();

	HASSERT(mlock_resource_is_locked(resource));
	mtfs_list_for_each_safe(tmp, pos, &resource->mlr_waiting) {
		lock = mtfs_list_entry(tmp, struct mlock, ml_res_link);
		ret = mlock_enqueue_try_nolock(lock);
		if (ret) {
			continue;
		}
		wake_up(&lock->ml_waitq);	
	}
	_HRETURN();
}

void mlock_cancel(struct mlock *lock)
{
	struct mlock_resource *resource = lock->ml_resource;
	HENTRY();

	HASSERT(lock->ml_state == MLOCK_STATE_GRANTED);

	mlock_resource_lock(resource);
	mlock_unlink(lock);
	mlock_resource_reprocess(resource);
	mlock_destroy(lock);
	mlock_resource_unlock(resource);

	_HRETURN();
}

#define MLOCK_TYPE_DEFAULT (&mlock_type_extent)

void mlock_resource_init(struct inode *inode)
{
	struct mlock_resource *resource = mtfs_i2resource(inode);
	int index = 0;
	HENTRY();

	MTFS_INIT_LIST_HEAD(&resource->mlr_granted);
	MTFS_INIT_LIST_HEAD(&resource->mlr_waiting);

	spin_lock_init(&resource->mlr_lock);
	resource->mlr_inode = inode;
	resource->mlr_type = MLOCK_TYPE_DEFAULT;

        /* initialize interval trees for each lock mode*/
        for (index = 0; index < MLOCK_MODE_NUM; index++) {
                resource->mlr_itree[index].mlit_size = 0;
                resource->mlr_itree[index].mlit_mode = 1 << index;
                resource->mlr_itree[index].mlit_root = NULL;
        }
        resource->mlr_inited = 1;
	_HRETURN();
}
