/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __HRFS_INODE_INTERNAL_H__
#define __HRFS_INODE_INTERNAL_H__
#include <hrfs_common.h>
#include <hrfs_inode.h>

extern struct inode_operations hrfs_main_iops;
extern struct inode_operations hrfs_dir_iops;
extern struct inode_operations hrfs_symlink_iops;

int hrfs_interpose(struct dentry *dentry, struct super_block *sb, int flag);
int hrfs_inode_dump(struct inode *inode);
struct dentry *hrfs_lookup_branch(struct dentry *dentry, hrfs_bindex_t bindex);

static inline struct hrfs_inode_info *hrfs_ii_alloc(void)
{
	struct hrfs_inode_info *i_info = NULL;

	HRFS_SLAB_ALLOC_PTR(i_info, hrfs_inode_info_cache);	
	return i_info;
}

static inline void hrfs_ii_free(struct hrfs_inode_info *i_info)
{
	HRFS_SLAB_FREE_PTR(i_info, hrfs_inode_info_cache);
	HASSERT(i_info == NULL);
}

/*
 * Calculate lower file size based on HRFS file size 
 * Only setattr uses it so we do not care about performance too much.
 */
static inline loff_t calculate_inode_size(struct inode *inode, int bindex, loff_t ia_size)
{
	loff_t low_ia_size = ia_size;

	return low_ia_size;
}
#endif /* __HRFS_INODE_INTERNAL_H__ */
