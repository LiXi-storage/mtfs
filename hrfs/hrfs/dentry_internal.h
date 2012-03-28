/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __HRFS_DENTRY_INTERNAL_H__
#define __HRFS_DENTRY_INTERNAL_H__
#include <hrfs_dentry.h>
#include "inode_internal.h"
#include "super_internal.h"
#include "main_internal.h"
extern struct dentry_operations hrfs_dops;

int hrfs_dentry_dump(struct dentry *dentry);

static inline int hrfs_d_alloc(struct dentry *dentry, hrfs_bindex_t bnum)
{
	struct hrfs_dentry_info *d_info = NULL;
	int ret = 0;
	
	HASSERT(dentry);
	HASSERT(bnum > 0 && bnum <= HRFS_BRANCH_MAX);
	HRFS_SLAB_ALLOC_PTR(d_info, hrfs_dentry_info_cache);
	if (unlikely(d_info == NULL)) {
		ret = -ENOMEM;
		goto out;
	}
	d_info->bnum = bnum;

	_hrfs_d2info(dentry) = d_info;
out:
	return ret;
}

static inline int hrfs_d_free(struct dentry *dentry)
{
	struct hrfs_dentry_info *d_info = NULL;
	int ret = 0;
	
	HASSERT(dentry);
	d_info = hrfs_d2info(dentry);
	HASSERT(d_info);
	
	HRFS_SLAB_FREE_PTR(d_info, hrfs_dentry_info_cache);
	_hrfs_d2info(dentry) = NULL;
	HASSERT(_hrfs_d2info(dentry) == NULL);
	return ret;
}

static inline int hrfs_d_is_alloced(struct dentry *dentry)
{
	if (dentry->d_fsdata == NULL) {
		return 0;
	}
	return 1;
}

static inline struct dentry *lock_parent(struct dentry *dentry)
{
	struct dentry *dir = NULL;
	
	HASSERT(dentry);
	HASSERT(dentry->d_parent);
	HASSERT(dentry->d_parent->d_inode);
	dir = dget(dentry->d_parent);
	mutex_lock(&dir->d_inode->i_mutex);
	return dir;
}

static inline struct dentry *get_parent(struct dentry *dentry) {
	return dget(dentry->d_parent);
}

static inline void unlock_dir(struct dentry *dir)
{
	HASSERT(dir);
	HASSERT(dir->d_inode);
	mutex_unlock(&dir->d_inode->i_mutex);
	dput(dir);
}

static inline int hrfs_d_is_positive(struct dentry *dentry)
{
	hrfs_bindex_t bindex = 0;
	struct dentry *hidden_dentry = NULL;
	hrfs_bindex_t bnum = hrfs_d2bnum(dentry);
	hrfs_bindex_t positive_bnum = 0;

	HASSERT(dentry);

	for (bindex = 0; bindex < bnum; bindex++) {
		hidden_dentry = hrfs_d2branch(dentry, bindex);
		if (hidden_dentry && hidden_dentry->d_inode) {
			positive_bnum++;
		}
	}
	return positive_bnum > 0;
}

static inline struct dentry *hrfs_d_root_branch(struct dentry *dentry, hrfs_bindex_t bindex)
{
	return hrfs_d2branch(dentry->d_sb->s_root, bindex);
}

static inline struct dentry *hrfs_d_recover_branch(struct dentry *dentry, hrfs_bindex_t bindex)
{
	return hrfs_s2brecover(dentry->d_sb, bindex);
}
#endif /* __HRFS_DENTRY_INTERNAL_H__ */
