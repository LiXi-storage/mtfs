/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __HRFS_DENTRY_H__
#define __HRFS_DENTRY_H__
#include <hrfs_common.h>
#include <linux/dcache.h>

struct hrfs_dentry_branch {
	__u64 invalid_flags;
	struct dentry *bdentry;
};

/* hrfs dentry data in memory */
struct hrfs_dentry_info {
	hrfs_bindex_t bnum;
	struct hrfs_dentry_branch barray[HRFS_BRANCH_MAX];
};

/* DO NOT access hrfs_*_dentry_t directly, use following macros */
#define _hrfs_d2info(dentry) ((dentry)->d_fsdata)
#define hrfs_d2info(dentry) ((struct hrfs_dentry_info *)_hrfs_d2info(dentry))
#define hrfs_d2bnum(dentry) (hrfs_d2info(dentry)->bnum)
#define hrfs_d2barray(dentry) (hrfs_d2info(dentry)->barray)
#define hrfs_d2branch(dentry, bindex) (hrfs_d2barray(dentry)[bindex].bdentry)
#define hrfs_d2binvalid(dentry, bindex) (hrfs_d2barray(dentry)[bindex].invalid_flags)

extern int hrfs_d_revalidate(struct dentry *dentry, struct nameidata *nd);
extern void hrfs_d_release(struct dentry *dentry);
#endif /* __HRFS_DENTRY_H__ */
