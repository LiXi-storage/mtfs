/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_DENTRY_H__
#define __MTFS_DENTRY_H__

#if defined(__linux__) && defined(__KERNEL__)
#include "mtfs_common.h"
#include "mtfs_super.h"
#include <linux/dcache.h>

struct mtfs_dentry_branch {
	__u64 invalid_flags;
	struct dentry *bdentry;
};

/* mtfs dentry data in memory */
struct mtfs_dentry_info {
	mtfs_bindex_t bnum;
	struct mtfs_dentry_branch barray[MTFS_BRANCH_MAX];
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

static inline struct dentry *mtfs_d_root_branch(struct dentry *dentry, mtfs_bindex_t bindex)
{
	return mtfs_d2branch(dentry->d_sb->s_root, bindex);
}

static inline struct dentry *mtfs_d_recover_branch(struct dentry *dentry, mtfs_bindex_t bindex)
{
	return mtfs_s2brecover(dentry->d_sb, bindex);
}

#else /* !defined (__linux__) && defined(__KERNEL__) */
#error This head is only for kernel space use
#endif /* !defined (__linux__) && defined(__KERNEL__) */

#endif /* __MTFS_DENTRY_H__ */
