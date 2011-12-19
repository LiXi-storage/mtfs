/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
 */

#ifndef __HRFS_SUPPORT_INTERNAL_H__
#define __HRFS_SUPPORT_INTERNAL_H__
#include <raid.h>
#include <hrfs_common.h>
#include <hrfs_support.h>
#include "hrfs_internal.h"

struct lowerfs_operations *lowerfs_get_ops(const char *type);
void lowerfs_put_ops(struct lowerfs_operations *fs_ops);

static inline int lowerfs_inode_set_flag(struct lowerfs_operations *fs_ops, struct inode *inode, __u32 flag)
{
	int ret = 0;
	if (fs_ops->lowerfs_inode_set_flag) {
		ret = fs_ops->lowerfs_inode_set_flag(inode, flag);
	}
	return ret;
}

static inline int lowerfs_inode_get_flag(struct lowerfs_operations *fs_ops, struct inode *inode, __u32 *hrfs_flag)
{
	int ret = 0;
	if (fs_ops->lowerfs_inode_get_flag) {
		ret = fs_ops->lowerfs_inode_get_flag(inode, hrfs_flag);
	}
	return ret;
}

static inline int lowerfs_inode_get_raid_type(struct lowerfs_operations *fs_ops, struct inode *inode, raid_type_t *raid_type)
{
	int ret = 0;
	__u32 hrfs_flag = 0;

	if (fs_ops->lowerfs_inode_get_flag) {
		ret = fs_ops->lowerfs_inode_get_flag(inode, &hrfs_flag);
		if (ret) {
			goto out;
		}
	}

	if (hrfs_flag & HRFS_FLAG_SETED) {
		*raid_type = hrfs_flag & HRFS_FLAG_RAID_MASK;
	} else {
		*raid_type = -1;
	}
out:
	return ret;
}

static inline int lowerfs_idata_init(struct lowerfs_operations *fs_ops, struct inode *inode, struct inode *parent_inode, int is_primary)
{
	int ret = 0;
	if (fs_ops->lowerfs_idata_init) {
		ret = fs_ops->lowerfs_idata_init(inode, parent_inode, is_primary);
	}
	return ret;
}

static inline int lowerfs_idata_finit(struct lowerfs_operations *fs_ops, struct inode *inode)
{
	int ret = 0;
	if (fs_ops->lowerfs_idata_finit) {
		ret = fs_ops->lowerfs_idata_finit(inode);
	}
	return ret;
}

#include "device_internal.h"
static inline struct lowerfs_operations *hrfs_sb2bops(struct super_block *sb, hrfs_bindex_t bindex)
{
	struct hrfs_device *device = hrfs_s2dev(sb);
	struct lowerfs_operations *lowerfs_ops = hrfs_dev2bops(device, bindex);
	return lowerfs_ops;
}

static inline struct lowerfs_operations *hrfs_i2bops(struct inode *inode, hrfs_bindex_t bindex)
{
	struct super_block *sb = inode->i_sb;
	return hrfs_sb2bops(sb, bindex);
}

static inline struct lowerfs_operations *hrfs_d2bops(struct dentry *dentry, hrfs_bindex_t bindex)
{
	struct super_block *sb = dentry->d_sb;
	return hrfs_sb2bops(sb, bindex);
}

static inline struct hrfs_operations *hrfs_sb2ops(struct super_block *sb)
{
	struct hrfs_device *device = hrfs_s2dev(sb);
	struct hrfs_operations *ops = hrfs_dev2ops(device);
	return ops;
}

static inline struct hrfs_operations *hrfs_i2ops(struct inode *inode)
{
	struct super_block *sb = inode->i_sb;
	return hrfs_sb2ops(sb);
}

static inline struct hrfs_operations *hrfs_d2ops(struct dentry *dentry)
{
	struct super_block *sb = dentry->d_sb;
	return hrfs_sb2ops(sb);
}
#endif /* __HRFS_SUPPORT_INTERNAL_H__ */
