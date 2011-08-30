/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
 */

#ifndef __HRFS_DENTRY_INTERNAL_H__
#define __HRFS_DENTRY_INTERNAL_H__
#include <hrfs_dentry.h>
#include "inode_internal.h"
extern struct dentry_operations hrfs_dops;

int hrfs_dentry_dump(struct dentry *dentry);

static inline hrfs_d_info_t *hrfs_di_alloc(void)
{
	hrfs_d_info_t *d_info = NULL;

	HRFS_SLAB_ALLOC_PTR(d_info, hrfs_dentry_info_cache);
	return d_info;
}

static inline void hrfs_di_free(hrfs_d_info_t *d_info)
{
	HRFS_SLAB_FREE_PTR(d_info, hrfs_dentry_info_cache);
	HASSERT(d_info == NULL);
}

static inline int hrfs_di_branch_alloc(hrfs_d_info_t *d_info, hrfs_bindex_t bnum)
{
	int ret = -ENOMEM;
	
	HASSERT(d_info);
	
	HRFS_ALLOC(d_info->barray, sizeof(*d_info->barray) * bnum);
	if (unlikely(d_info->barray == NULL)) {
		goto out;
	}

	d_info->bnum = bnum;
	ret = 0;	
out:
	return ret;
}

static inline int hrfs_di_branch_free(hrfs_d_info_t *d_info)
{
	int ret = 0;
	
	HASSERT(d_info);
	HASSERT(d_info->barray);
	
	HRFS_FREE(d_info->barray, sizeof(*d_info->barray) * d_info->bnum);
	
	//i_info->ii_branch = NULL; /* HRFS_FREE will do this */
	HASSERT(d_info->barray == NULL);
	return ret;
}

static inline int hrfs_d_alloc(struct dentry *dentry, hrfs_bindex_t bnum)
{
	hrfs_d_info_t *d_info = NULL;
	int ret = 0;
	
	HASSERT(dentry);
	
	d_info = hrfs_di_alloc();
	if (unlikely(d_info == NULL)) {
		ret = -ENOMEM;
		goto out;
	}
	
	ret = hrfs_di_branch_alloc(d_info, bnum);
	if (unlikely(ret)) {
		goto out_dinfo;
	}
	
	_hrfs_d2info(dentry) = d_info;
	ret = 0;
	goto out;
out_dinfo:
	hrfs_di_free(d_info);
out:
	return ret;
}

static inline int hrfs_d_free(struct dentry *dentry)
{
	hrfs_d_info_t *d_info = NULL;
	int ret = 0;
	
	HASSERT(dentry);
	d_info = hrfs_d2info(dentry);
	HASSERT(d_info);
	
	hrfs_di_branch_free(d_info);
	
	hrfs_di_free(d_info);
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

static inline hrfs_bindex_t hrfs_d_choose_bindex(struct dentry *dentry, __u64 valid_flags)
{
	hrfs_bindex_t bindex = -1;

	HASSERT(dentry);
	HASSERT(hrfs_valid_flags_is_valid(valid_flags));
	
	if (valid_flags == HRFS_DENTRY_VALID) {
		for (bindex = 0; bindex < hrfs_d2bnum(dentry); bindex++) {
			if ((hrfs_d2binvalid(dentry, bindex) & HRFS_DENTRY_VALID) == 0) {
				HASSERT(hrfs_d2branch(dentry, bindex));
				goto out;
			}
		}
		bindex = -1;
	} else if (dentry->d_inode) {
		bindex = hrfs_i_choose_bindex(dentry->d_inode, valid_flags);
	}

out:	
	return bindex;
}

static inline struct dentry *hrfs_d_choose_branch(struct dentry *dentry, __u64 valid_flags)
{
	hrfs_bindex_t bindex = hrfs_d_choose_bindex(dentry, valid_flags);
	struct dentry *hidden_dentry = NULL;

	if (bindex >=0 && bindex < hrfs_d2bnum(dentry)) {
		hidden_dentry = hrfs_d2branch(dentry, bindex);
	}

	if (hidden_dentry == NULL) {
		HDEBUG("failed to choose a valid branch, valid_flags = %llu\n", valid_flags);
	}
	return hidden_dentry;
}

#endif /* __HRFS_DENTRY_INTERNAL_H__ */
