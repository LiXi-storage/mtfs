/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
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

static inline hrfs_i_info_t *hrfs_ii_alloc(void)
{
	hrfs_i_info_t *i_info = NULL;

	HRFS_SLAB_ALLOC_PTR(i_info, hrfs_inode_info_cache);	
	return i_info;
}

static inline void hrfs_ii_free(hrfs_i_info_t *i_info)
{
	HRFS_SLAB_FREE_PTR(i_info, hrfs_inode_info_cache);
	HASSERT(i_info == NULL);
}

static inline int hrfs_ii_branch_alloc(hrfs_i_info_t *i_info, hrfs_bindex_t bnum)
{
	int ret = -ENOMEM;
	
	HASSERT(i_info);

	HRFS_ALLOC_GFP(i_info->barray, sizeof(*i_info->barray) * bnum, GFP_ATOMIC);
	if (unlikely(i_info->barray == NULL)) {
		goto out;
	}

	i_info->bnum = bnum;
	ret = 0;

out:
	return ret;
}

static inline int hrfs_ii_branch_free(hrfs_i_info_t *i_info)
{
	int ret = 0;
	
	HASSERT(i_info);
	HASSERT(i_info->barray);
	
	HRFS_FREE(i_info->barray, sizeof(*i_info->barray) * i_info->bnum);
	
	//i_info->ii_branch = NULL; /* HRFS_FREE will do this */
	HASSERT(i_info->barray == NULL);
	return ret;
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
