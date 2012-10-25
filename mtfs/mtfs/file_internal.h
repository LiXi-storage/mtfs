/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_FILE_INTERNAL_H__
#define __MTFS_FILE_INTERNAL_H__
#include <mtfs_file.h>
#include <mtfs_io.h>

#ifdef HAVE_DENTRY_OPEN_4ARGS
#include <linux/cred.h>
#define mtfs_dentry_open(dentry, mnt, flags, cred) dentry_open(dentry, mnt, flags, cred)
#else /* !HAVE_DENTRY_OPEN_4ARGS */
#define mtfs_dentry_open(dentry, mnt, flags, cred) dentry_open(dentry, mnt, flags)
#endif /* !HAVE_DENTRY_OPEN_4ARGS */

#define READ 0
#define WRITE 1
ssize_t _do_read_write(int is_write, struct file *file, void *buf, ssize_t count, loff_t *ppos);
ssize_t mtfs_file_rw_branch(int is_write, struct file *file, const struct iovec *iov,
                            unsigned long nr_segs, loff_t *ppos, mtfs_bindex_t bindex);
int mtfs_io_init_rw_common(struct mtfs_io *io, int is_write,
                           struct file *file, const struct iovec *iov,
                           unsigned long nr_segs, loff_t *ppos, size_t rw_size);
size_t get_iov_count(const struct iovec *iov,
                     unsigned long *nr_segs);
#endif /* __MTFS_FILE_INTERNAL_H__ */
