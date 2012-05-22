/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __HRFS_SWGFS_SUPPORT_H__
#define __HRFS_SWGFS_SUPPORT_H__
#if defined (__linux__) && defined(__KERNEL__)
#include <mtfs_file.h>

#include <swgfs/swgfs_user.h>
extern ssize_t ll_mtfs_file_sendfile(struct file *parent_file, int b_remain, mtfs_f_branch_t *barray, loff_t *ppos,
                                size_t count, read_actor_t actor, void *target);
extern ssize_t ll_mtfs_file_writev(struct file *parent_file, int b_remain, mtfs_f_branch_t *barray, const struct iovec *iov,
                              unsigned long nr_segs, loff_t *ppos);
extern ssize_t ll_mtfs_file_readv(struct file *parent_file, int b_remain, mtfs_f_branch_t *barray, const struct iovec *iov,
                              unsigned long nr_segs, loff_t *ppos);

/*
 * Function: int swgfs_getstripe(struct dentry *dentry, struct lov_user_md **lumpp)
 * Purpose : get a dentry's stripe
 * Params  : dentry - denry to getstripe, must be a regular file or directory's denry
 *         : lumpp - pointer to save stripe data
 * Returns : length of memory malloced
 */
extern int swgfs_getstripe(struct dentry *dentry, struct lov_user_md **lumpp, int lumlen);
extern void swgfs_getstripe_finished(struct lov_user_md *lump, int lumlen);

extern struct lowerfs_operations lowerfs_swgfs_ops;
#define SWGFS_SUPER_MAGIC 0x0BD00BD1

#endif /* defined (__linux__) && defined(__KERNEL__) */
#endif /* __HRFS_SWGFS_SUPPORT_H__ */
