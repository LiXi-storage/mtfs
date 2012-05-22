/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __HRFS_DENTRY_H__
#define __HRFS_DENTRY_H__
#include "mtfs_common.h"
#include <linux/dcache.h>

struct mtfs_dentry_branch {
	__u64 invalid_flags;
	struct dentry *bdentry;
};

/* mtfs dentry data in memory */
struct mtfs_dentry_info {
	mtfs_bindex_t bnum;
	struct mtfs_dentry_branch barray[HRFS_BRANCH_MAX];
};

/* DO NOT access mtfs_*_dentry_t directly, use following macros */
#define _mtfs_d2info(dentry) ((dentry)->d_fsdata)
#define mtfs_d2info(dentry) ((struct mtfs_dentry_info *)_mtfs_d2info(dentry))
#define mtfs_d2bnum(dentry) (mtfs_d2info(dentry)->bnum)
#define mtfs_d2barray(dentry) (mtfs_d2info(dentry)->barray)
#define mtfs_d2branch(dentry, bindex) (mtfs_d2barray(dentry)[bindex].bdentry)
#define mtfs_d2binvalid(dentry, bindex) (mtfs_d2barray(dentry)[bindex].invalid_flags)

extern int mtfs_d_revalidate(struct dentry *dentry, struct nameidata *nd);
extern void mtfs_d_release(struct dentry *dentry);
#endif /* __HRFS_DENTRY_H__ */
