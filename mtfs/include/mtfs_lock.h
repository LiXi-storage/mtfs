/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_LOCK_H__
#define __MTFS_LOCK_H__

#if defined(__linux__) && defined(__KERNEL__)

#include <linux/spinlock.h>
#include <mtfs_list.h>

/* lock modes */
typedef enum {
	MLOCK_MODE_MIN   = 0,
	MLOCK_MODE_READ  = 1,
	MLOCK_MODE_WRITE = 2,
        MLOCK_MODE_NULL  = 4,
        MLOCK_MODE_MAX
} mlock_mode_t;

/* lock types */
typedef enum {
	MLOCK_TYPE_MIN    = 0,
        MLOCK_TYPE_PLAIN  = 1,
        MLOCK_TYPE_EXTENT = 2,
        MLOCK_TYPE_IBITS  = 3,
        MLOCK_TYPE_MAX
} mlock_type_t;

#define MLOCK_MODE_NUM 3

#define MLOCK_COMPAT_WRITE (MLOCK_MODE_NULL)
#define MLOCK_COMPAT_READ  (MLOCK_COMPAT_WRITE | MLOCK_MODE_READ)
#define MLOCK_COMPAT_NULL  (MLOCK_COMPAT_READ)

extern mlock_mode_t mlock_compat_array[];

static inline int mlock_mode_compat(mlock_mode_t exist_mode, mlock_mode_t new_mode)
{
       return (mlock_compat_array[exist_mode] & new_mode);
}

struct mlock_resource {
	mtfs_list_t            mlr_granted; /* Queue of granted locks */
	mtfs_list_t            mlr_waiting; /* Queue of waiting locks */
	spinlock_t             mlr_lock;    /* Lock */
	struct inode          *mlr_inode;   /* Inode this resource belongs to */
	int                    mlr_inited;  /* Resource is inited */
	mlock_type_t           mlr_type;    /* Resource type */
};

static inline void mlock_resource_lock(struct mlock_resource *resource)
{
	spin_lock(&resource->mlr_lock);
}

static inline void mlock_resource_unlock(struct mlock_resource *resource)
{
	spin_unlock(&resource->mlr_lock);
}

static inline int mlock_resource_is_locked(struct mlock_resource *resource)
{
	return spin_is_locked(&resource->mlr_lock);
}

/* lock state */
typedef enum {
	MLOCK_STATE_NEW      = 0,
        MLOCK_STATE_WAITING  = 1,
        MLOCK_STATE_GRANTED  = 2,
} mlock_state_t;

struct mlock {
	struct mlock_resource *ml_resource;  /* Resource to protect */
	mlock_mode_t           ml_mode;      /* Lock mode */
	mlock_type_t           ml_type;      /* Lock type */
	mtfs_list_t            ml_res_link;  /* Linkage to resource's lock queues */
	mtfs_list_t            ml_mode_link; /* Linkage to the skiplist of mode group */
	mlock_state_t          ml_state;     /* Lock state */
	wait_queue_head_t      ml_waitq;     /* Process waiting for the lock */
	pid_t                  ml_pid;       /* Pid which created this lock */
};

static inline int mlock_is_granted(struct mlock *lock)
{
	int ret = 0;

	mlock_resource_lock(lock->ml_resource);
	ret = (lock->ml_state == MLOCK_STATE_GRANTED);
	mlock_resource_unlock(lock->ml_resource);

	return ret;
}

extern void mlock_cancel(struct mlock *lock);
extern struct mlock *mlock_enqueue(struct inode *inode, mlock_mode_t mode);

#else /* !defined (__linux__) && defined(__KERNEL__) */
#error This head file is only for kernel space use
#endif /* !defined (__linux__) && defined(__KERNEL__) */

#endif /* __MLOCK_H__ */
