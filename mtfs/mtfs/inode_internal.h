/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_INODE_INTERNAL_H__
#define __MTFS_INODE_INTERNAL_H__

#include <mtfs_common.h>
#include <mtfs_inode.h>
#include "main_internal.h"

extern struct inode_operations mtfs_main_iops;
extern struct inode_operations mtfs_dir_iops;
extern struct inode_operations mtfs_symlink_iops;

struct dentry *mtfs_lookup_branch(struct dentry *dentry, mtfs_bindex_t bindex);
int mtfs_interpose(struct dentry *dentry, struct super_block *sb, int flag);
int mtfs_inode_dump(struct inode *inode);
int mtfs_inode_set(struct inode *inode, void *dentry);

static inline struct mtfs_inode_info *mtfs_ii_alloc(void)
{
	struct mtfs_inode_info *i_info = NULL;

	MTFS_SLAB_ALLOC_PTR(i_info, mtfs_inode_info_cache);	
	return i_info;
}

static inline void mtfs_ii_free(struct mtfs_inode_info *i_info)
{
	MTFS_SLAB_FREE_PTR(i_info, mtfs_inode_info_cache);
	HASSERT(i_info == NULL);
}

/*
 * Calculate lower file size based on MTFS file size 
 * Only setattr uses it so we do not care about performance too much.
 */
static inline loff_t calculate_inode_size(struct inode *inode, int bindex, loff_t ia_size)
{
	loff_t low_ia_size = ia_size;

	return low_ia_size;
}
#endif /* __MTFS_INODE_INTERNAL_H__ */
