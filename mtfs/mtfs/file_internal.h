/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_FILE_INTERNAL_H__
#define __MTFS_FILE_INTERNAL_H__
#include <mtfs_file.h>

ssize_t mtfs_file_rw_branch(int is_write, struct file *file, const struct iovec *iov,
                            unsigned long nr_segs, loff_t *ppos, mtfs_bindex_t bindex);
#endif /* __MTFS_FILE_INTERNAL_H__ */
