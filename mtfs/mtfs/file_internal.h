/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_FILE_INTERNAL_H__
#define __MTFS_FILE_INTERNAL_H__
#include <mtfs_file.h>
#include <mtfs_io.h>

ssize_t mtfs_file_rw_branch(int is_write, struct file *file, const struct iovec *iov,
                            unsigned long nr_segs, loff_t *ppos, mtfs_bindex_t bindex);
int mtfs_io_init_rw_common(struct mtfs_io *io, int is_write,
                           struct file *file, const struct iovec *iov,
                           unsigned long nr_segs, loff_t *ppos, size_t rw_size);
int mtfs_readdir_branch(struct file *file,
                        void *dirent,
                        filldir_t filldir,
                        mtfs_bindex_t bindex);
int mtfs_open_branch(struct inode *inode,
                     struct file *file,
                     mtfs_bindex_t bindex);
size_t get_iov_count(const struct iovec *iov,
                     unsigned long *nr_segs);
#endif /* __MTFS_FILE_INTERNAL_H__ */
