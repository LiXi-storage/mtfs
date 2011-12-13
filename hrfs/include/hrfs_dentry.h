/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
 */

#ifndef __HRFS_DENTRY_H__
#define __HRFS_DENTRY_H__
#include <hrfs_common.h>
#include <linux/dcache.h>

typedef struct hrfs_dentry_branch {
	__u64 invalid_flags;
	struct dentry *bdentry;
} hrfs_d_branch_t;

/* hrfs dentry data in memory */
typedef struct hrfs_dentry_info {
	hrfs_bindex_t bnum;
	hrfs_d_branch_t *barray; /*TODO: change to barray[] */
} hrfs_d_info_t;

/* DO NOT access hrfs_*_dentry_t directly, use following macros */
#define _hrfs_d2info(dentry) ((dentry)->d_fsdata)
#define hrfs_d2info(dentry) ((hrfs_d_info_t *)_hrfs_d2info(dentry))
#define hrfs_d2bnum(dentry) (hrfs_d2info(dentry)->bnum)
#define hrfs_d2barray(dentry) (hrfs_d2info(dentry)->barray)
#define hrfs_d2branch(dentry, bindex) (hrfs_d2barray(dentry)[bindex].bdentry)
#define hrfs_d2binvalid(dentry, bindex) (hrfs_d2barray(dentry)[bindex].invalid_flags)

extern int hrfs_d_revalidate(struct dentry *dentry, struct nameidata *nd);
extern void hrfs_d_release(struct dentry *dentry);
#endif /* __HRFS_DENTRY_H__ */
