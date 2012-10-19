/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_SUPER_INTERNAL_H__
#define __MTFS_SUPER_INTERNAL_H__
#include <memory.h>
#include <debug.h>
#include <mtfs_super.h>
#include "main_internal.h"
extern struct super_operations mtfs_sops;

#define s_type(sb) ((sb)->s_type->name)

/* fist file systems superblock magic */
#define MTFS_SUPER_MAGIC 0xc0ffee

/* mtfs root inode number */
#define MTFS_ROOT_INO	 1

static inline int mtfs_s_alloc(struct super_block *sb, mtfs_bindex_t bnum)
{
	struct mtfs_sb_info *s_info = NULL;
	int ret = 0;

	MASSERT(sb);
	MASSERT(bnum > 0 && bnum <= MTFS_BRANCH_MAX);

	MTFS_SLAB_ALLOC_PTR(s_info, mtfs_sb_info_cache);
	if (unlikely(s_info == NULL)) {
		ret = -ENOMEM;
		goto out;
	}
	_mtfs_s2info(sb) = s_info;

	mtfs_s2bnum(sb) = bnum;
out:
	return ret;
}

static inline int mtfs_s_free(struct super_block *sb)
{
	struct mtfs_sb_info *s_info = NULL;
	int ret = 0;

	MASSERT(sb);
	s_info = mtfs_s2info(sb);
	MASSERT(s_info);

	MTFS_SLAB_FREE_PTR(s_info, mtfs_sb_info_cache);
	_mtfs_s2info(sb) = NULL;
	MASSERT(_mtfs_s2info(sb) == NULL);
	return ret;
}

static inline void *mtfs_i2subinfo(struct inode *inode)
{
	struct super_block *sb = inode->i_sb;
	return mtfs_s2subinfo(sb);
}

static inline void *mtfs_d2subinfo(struct dentry *dentry)
{
	struct super_block *sb = dentry->d_sb;
	return mtfs_s2subinfo(sb);
}

static inline void *mtfs_f2subinfo(struct file *file)
{
	struct dentry *dentry = file->f_dentry;
	return mtfs_d2subinfo(dentry);
}
#endif /* __MTFS_SUPER_INTERNAL_H__ */
