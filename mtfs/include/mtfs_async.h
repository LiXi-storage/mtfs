/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_ASYNC_H__
#define __MTFS_ASYNC_H__

#if defined(__linux__) && defined(__KERNEL__)

#include <mtfs_interval_tree.h>
#include <spinlock.h>

struct masync_bucket {
        int                         mab_size;  /* Number of dirty extent */
        struct mtfs_interval_node  *mab_root;  /* Actual tree */
        mtfs_spinlock_t             mab_lock;  /* Lock */
};

extern void masync_bucket_init(struct masync_bucket *bucket);
extern void masync_bucket_add(struct masync_bucket *bucket,
                              struct mtfs_interval_node_extent *extent);

#else /* !defined (__linux__) && defined(__KERNEL__) */
#error This head is only for kernel space use
#endif /* !defined (__linux__) && defined(__KERNEL__) */

#endif /* __MTFS_ASYNC_H__ */
