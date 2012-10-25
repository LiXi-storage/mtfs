/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_ASYNC_H__
#define __MTFS_ASYNC_H__

#if defined(__linux__) && defined(__KERNEL__)
#include <linux/file.h>
#include <linux/poll.h>
#include <mtfs_interval_tree.h>
#include <spinlock.h>

struct masync_bucket {
        int                         mab_size;  /* Number of dirty extent */
        struct mtfs_interval_node  *mab_root;  /* Actual tree */
        mtfs_spinlock_t             mab_lock;  /* Lock */
};

#ifdef HAVE_FILE_READV
extern ssize_t masync_file_readv(struct file *file, const struct iovec *iov,
                                 unsigned long nr_segs, loff_t *ppos);
#else /* ! HAVE_FILE_READV */
extern ssize_t masync_file_aio_read(struct kiocb *iocb, const struct iovec *iov,
                                    unsigned long nr_segs, loff_t pos);
#endif /* ! HAVE_FILE_WRITEV */

#ifdef HAVE_FILE_WRITEV
extern ssize_t masync_file_writev(struct file *file, const struct iovec *iov,
                                  unsigned long nr_segs, loff_t *ppos);
#else /* ! HAVE_FILE_WRITEV */
extern ssize_t masync_file_aio_write(struct kiocb *iocb, const struct iovec *iov,
                                     unsigned long nr_segs, loff_t pos);
#endif /* ! HAVE_FILE_WRITEV */

#else /* !defined (__linux__) && defined(__KERNEL__) */
#error This head is only for kernel space use
#endif /* !defined (__linux__) && defined(__KERNEL__) */

#endif /* __MTFS_ASYNC_H__ */
