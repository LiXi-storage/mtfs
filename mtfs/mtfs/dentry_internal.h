/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_DENTRY_INTERNAL_H__
#define __MTFS_DENTRY_INTERNAL_H__
#include <mtfs_dentry.h>
#include "inode_internal.h"
#include "super_internal.h"
#include "main_internal.h"
extern struct dentry_operations mtfs_dops;

int mtfs_dentry_dump(struct dentry *dentry);

static inline int mtfs_d_alloc(struct dentry *dentry, mtfs_bindex_t bnum)
{
	struct mtfs_dentry_info *d_info = NULL;
	int ret = 0;
	
	MASSERT(dentry);
	MASSERT(bnum > 0 && bnum <= MTFS_BRANCH_MAX);
	MTFS_SLAB_ALLOC_PTR(d_info, mtfs_dentry_info_cache);
	if (unlikely(d_info == NULL)) {
		ret = -ENOMEM;
		goto out;
	}
	d_info->bnum = bnum;

	_mtfs_d2info(dentry) = d_info;
out:
	return ret;
}

static inline int mtfs_d_free(struct dentry *dentry)
{
	struct mtfs_dentry_info *d_info = NULL;
	int ret = 0;
	
	MASSERT(dentry);
	d_info = mtfs_d2info(dentry);
	MASSERT(d_info);
	
	MTFS_SLAB_FREE_PTR(d_info, mtfs_dentry_info_cache);
	_mtfs_d2info(dentry) = NULL;
	MASSERT(_mtfs_d2info(dentry) == NULL);
	return ret;
}

static inline int mtfs_d_is_alloced(struct dentry *dentry)
{
	if (dentry->d_fsdata == NULL) {
		return 0;
	}
	return 1;
}

static inline struct dentry *lock_parent(struct dentry *dentry)
{
	struct dentry *dir = NULL;
	
	MASSERT(dentry);
	MASSERT(dentry->d_parent);
	MASSERT(dentry->d_parent->d_inode);
	dir = dget(dentry->d_parent);
	mutex_lock(&dir->d_inode->i_mutex);
	return dir;
}

static inline struct dentry *get_parent(struct dentry *dentry) {
	return dget(dentry->d_parent);
}

static inline void unlock_dir(struct dentry *dir)
{
	MASSERT(dir);
	MASSERT(dir->d_inode);
	mutex_unlock(&dir->d_inode->i_mutex);
	dput(dir);
}

static inline int mtfs_d_is_positive(struct dentry *dentry)
{
	mtfs_bindex_t bindex = 0;
	struct dentry *hidden_dentry = NULL;
	mtfs_bindex_t bnum = mtfs_d2bnum(dentry);
	mtfs_bindex_t positive_bnum = 0;

	MASSERT(dentry);

	for (bindex = 0; bindex < bnum; bindex++) {
		hidden_dentry = mtfs_d2branch(dentry, bindex);
		if (hidden_dentry && hidden_dentry->d_inode) {
			positive_bnum++;
		}
	}
	return positive_bnum > 0;
}

#define lock_dentry(___dentry)          spin_lock(&(___dentry)->d_lock)
#define unlock_dentry(___dentry)        spin_unlock(&(___dentry)->d_lock)

#endif /* __MTFS_DENTRY_INTERNAL_H__ */
