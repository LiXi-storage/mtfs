/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_LOWERFS_INTERNAL_H__
#define __MTFS_LOWERFS_INTERNAL_H__
#include <raid.h>
#include <mtfs_common.h>
#include <mtfs_lowerfs.h>
#include <mtfs_flag.h>

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

static inline int lowerfs_inode_get_flag(struct lowerfs_operations *fs_ops, struct inode *inode, __u32 *mtfs_flag)
{
	int ret = 0;
	if (fs_ops->lowerfs_inode_get_flag) {
		ret = fs_ops->lowerfs_inode_get_flag(inode, mtfs_flag);
	}
	return ret;
}

/*
 * Change to a atomic operation.
 * Send a command, let sever make it atomic.
 */
static inline int lowerfs_inode_invalidate_data(struct lowerfs_operations *fs_ops, struct inode *inode)
{
	int ret = 0;
	__u32 mtfs_flag = 0;

	if (fs_ops->lowerfs_inode_get_flag == NULL ||
	    fs_ops->lowerfs_inode_set_flag == NULL) {
		ret = -EOPNOTSUPP;
		goto out;
	}

	ret = fs_ops->lowerfs_inode_get_flag(inode, &mtfs_flag);
	if (ret) {
		goto out;
	}
	mtfs_flag |= MTFS_FLAG_DATABAD | MTFS_FLAG_SETED;

	ret = fs_ops->lowerfs_inode_set_flag(inode, mtfs_flag);
out:
	return ret;
}

static inline int lowerfs_inode_get_raid_type(struct lowerfs_operations *fs_ops, struct inode *inode, raid_type_t *raid_type)
{
	int ret = 0;
	__u32 mtfs_flag = 0;

	if (fs_ops->lowerfs_inode_get_flag) {
		ret = fs_ops->lowerfs_inode_get_flag(inode, &mtfs_flag);
		if (ret) {
			goto out;
		}
	}

	if (mtfs_flag & MTFS_FLAG_SETED) {
		*raid_type = mtfs_flag & MTFS_FLAG_RAID_MASK;
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
#include "super_internal.h"
static inline struct lowerfs_operations *mtfs_s2bops(struct super_block *sb, mtfs_bindex_t bindex)
{
	struct mtfs_device *device = mtfs_s2dev(sb);
	struct lowerfs_operations *lowerfs_ops = mtfs_dev2bops(device, bindex);
	return lowerfs_ops;
}

static inline struct lowerfs_operations *mtfs_i2bops(struct inode *inode, mtfs_bindex_t bindex)
{
	struct super_block *sb = inode->i_sb;
	return mtfs_s2bops(sb, bindex);
}

static inline struct lowerfs_operations *mtfs_d2bops(struct dentry *dentry, mtfs_bindex_t bindex)
{
	struct super_block *sb = dentry->d_sb;
	return mtfs_s2bops(sb, bindex);
}

static inline struct mtfs_operations *mtfs_s2ops(struct super_block *sb)
{
	struct mtfs_device *device = mtfs_s2dev(sb);
	struct mtfs_operations *ops = mtfs_dev2ops(device);
	return ops;
}

static inline struct mtfs_operations *mtfs_i2ops(struct inode *inode)
{
	struct super_block *sb = inode->i_sb;
	return mtfs_s2ops(sb);
}

static inline struct mtfs_operations *mtfs_d2ops(struct dentry *dentry)
{
	struct super_block *sb = dentry->d_sb;
	return mtfs_s2ops(sb);
}

static inline struct mtfs_operations *mtfs_f2ops(struct file *file)
{
	struct dentry *dentry = file->f_dentry;
	return mtfs_d2ops(dentry);
}
#endif /* __MTFS_LOWERFS_INTERNAL_H__ */
