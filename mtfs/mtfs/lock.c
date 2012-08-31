/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

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

struct mlock_skiplist_position {
	mtfs_list_t *res_link;
	mtfs_list_t *mode_link;
};

static void mlock_skiplist_search_prev(mtfs_list_t *queue, struct mlock *req_lock, struct mlock_skiplist_position *prev)
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

static void mlock_skiplist_unlink(struct mlock *lock)
{
	HENTRY();

	HASSERT(mlock_resource_is_locked(lock->ml_resource));
	mtfs_list_del_init(&lock->ml_res_link);
	mtfs_list_del_init(&lock->ml_mode_link);

	_HRETURN();
}

static void mlock_skiplist_add(struct mlock *lock, struct mlock_skiplist_position *prev)
{
	HENTRY();

	HASSERT(mlock_resource_is_locked(lock->ml_resource));
	HASSERT(lock->ml_state == MLOCK_STATE_GRANTED);
        HASSERT(mtfs_list_empty(&lock->ml_res_link));
        HASSERT(mtfs_list_empty(&lock->ml_mode_link));

        mtfs_list_add(&lock->ml_res_link, prev->res_link);
        mtfs_list_add(&lock->ml_mode_link, prev->mode_link);	
	_HRETURN();
}

/*
 * 0 if the lock is compatible
 * 1 if the lock is not compatible
 */
static int mlock_queue_conflict(mtfs_list_t *queue, struct mlock *req_lock)
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

#if 0
		/* last lock in mode group */
		tmp = &mtfs_list_entry(old_lock->ml_mode_link.prev,
                                       struct mlock,
                                       ml_mode_link)->ml_res_link;
#endif

		if (mlock_mode_compat(old_lock->ml_mode, req_lock->ml_mode)) {
			continue;
		}

		ret = 1;
		break;
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

static void mlock_grant(struct mlock *lock)
{
	struct mlock_resource *resource = lock->ml_resource;
	struct mlock_skiplist_position prev;
	HENTRY();

	HASSERT(mlock_resource_is_locked(resource));
	lock->ml_state = MLOCK_STATE_GRANTED;

	mlock_skiplist_unlink(lock);
	mlock_skiplist_search_prev(&resource->mlr_granted, lock, &prev);
	mlock_skiplist_add(lock, &prev);
	_HRETURN();
}

static int mlock_enqueue_try_nolock(struct mlock *lock)
{
	struct mlock_resource *resource = lock->ml_resource;
	int ret = 0;
	HENTRY();

	HASSERT(mlock_resource_is_locked(resource));
	HASSERT(lock->ml_mode > MLOCK_MODE_MIN && lock->ml_mode < MLOCK_MODE_MAX);

	ret = mlock_queue_conflict(&resource->mlr_granted, lock);
	if (ret) {
		HDEBUG("lock %p conflicting with granted queue\n", lock);
		goto out_pend;
	}

	ret = mlock_queue_conflict(&resource->mlr_waiting, lock);
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


static struct mlock *mlock_create(struct mlock_resource *resource, mlock_mode_t mode)
{
	struct mlock *lock = NULL;
	HENTRY();

	MTFS_SLAB_ALLOC_PTR(lock, mtfs_lock_cache);
	if (lock == NULL) {
		HERROR("failed to create lock, not enough memory\n");
		goto out;
	}

	lock->ml_resource = resource;
	lock->ml_mode = mode;
	lock->ml_type = resource->mlr_type;
	MTFS_INIT_LIST_HEAD(&lock->ml_res_link);
	MTFS_INIT_LIST_HEAD(&lock->ml_mode_link);
	lock->ml_state = MLOCK_STATE_NEW;
	init_waitqueue_head(&lock->ml_waitq);
	lock->ml_pid = current->pid;
out:
	HRETURN(lock);
}

struct mlock *mlock_enqueue(struct inode *inode, mlock_mode_t mode)
{
	struct mlock_resource *resource = mtfs_i2resource(inode);
	int ret = 0;
	struct mlock *lock = NULL;
	HENTRY();

	lock = mlock_create(resource, mode);
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

void mlock_resource_init(struct inode *inode)
{
	struct mlock_resource *resource = mtfs_i2resource(inode);
	HENTRY();

	MTFS_INIT_LIST_HEAD(&resource->mlr_granted);
	MTFS_INIT_LIST_HEAD(&resource->mlr_waiting);

	spin_lock_init(&resource->mlr_lock);
	resource->mlr_inode = inode;

        resource->mlr_inited = 1;
	_HRETURN();
}


void mlock_destroy(struct mlock *lock)
{
	struct mlock_resource *resource = lock->ml_resource;
	HENTRY();

	HASSERT(mlock_resource_is_locked(resource));
	HASSERT(mtfs_list_empty(&lock->ml_res_link));
	HASSERT(mtfs_list_empty(&lock->ml_mode_link));

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

	mlock_resource_lock(resource);
	mlock_skiplist_unlink(lock);
	mlock_resource_reprocess(resource);
	mlock_destroy(lock);
	mlock_resource_unlock(resource);

	_HRETURN();
}



void mlock_dump(struct mlock *lock)
{

}
