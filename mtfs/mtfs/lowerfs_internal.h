/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_LOWERFS_INTERNAL_H__
#define __MTFS_LOWERFS_INTERNAL_H__
#include <raid.h>
#include <mtfs_common.h>
#include <mtfs_lowerfs.h>
#include <mtfs_flag.h>

int mlowerfs_bucket_init(struct mlowerfs_bucket *bucket);
int mlowerfs_bucket_read(struct mlowerfs_bucket *bucket);
struct mtfs_lowerfs *mlowerfs_get(const char *type);
void mlowerfs_put(struct mtfs_lowerfs *fs_ops);

static inline int mlowerfs_setflag(struct mtfs_lowerfs *fs_ops, struct inode *inode, __u32 flag)
{
	int ret = 0;
	if (fs_ops->ml_setflag) {
		ret = fs_ops->ml_setflag(inode, flag);
	}
	return ret;
}

static inline int mlowerfs_getflag(struct mtfs_lowerfs *fs_ops, struct inode *inode, __u32 *mtfs_flag)
{
	int ret = 0;
	if (fs_ops->ml_getflag) {
		ret = fs_ops->ml_getflag(inode, mtfs_flag);
	}
	return ret;
}

/*
 * Change to a atomic operation.
 * Send a command, let sever make it atomic.
 */
static inline int mlowerfs_invalidate_data(struct mtfs_lowerfs *fs_ops, struct inode *inode)
{
	int ret = 0;
	__u32 mtfs_flag = 0;

	if (fs_ops->ml_getflag == NULL ||
	    fs_ops->ml_setflag == NULL) {
		ret = -EOPNOTSUPP;
		goto out;
	}

	ret = fs_ops->ml_getflag(inode, &mtfs_flag);
	if (ret) {
		goto out;
	}
	mtfs_flag |= MTFS_FLAG_DATABAD | MTFS_FLAG_SETED;

	ret = fs_ops->ml_setflag(inode, mtfs_flag);
out:
	return ret;
}

static inline int mlowerfs_get_raid_type(struct mtfs_lowerfs *fs_ops, struct inode *inode, raid_type_t *raid_type)
{
	int ret = 0;
	__u32 mtfs_flag = 0;

	if (fs_ops->ml_getflag) {
		ret = fs_ops->ml_getflag(inode, &mtfs_flag);
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

#include "device_internal.h"
#include "super_internal.h"
static inline struct mtfs_lowerfs *mtfs_s2bops(struct super_block *sb, mtfs_bindex_t bindex)
{
	struct mtfs_device *device = mtfs_s2dev(sb);
	struct mtfs_lowerfs *lowerfs = mtfs_dev2bops(device, bindex);
	return lowerfs;
}

static inline struct mtfs_lowerfs *mtfs_i2bops(struct inode *inode, mtfs_bindex_t bindex)
{
	struct super_block *sb = inode->i_sb;
	return mtfs_s2bops(sb, bindex);
}

static inline struct mtfs_lowerfs *mtfs_d2bops(struct dentry *dentry, mtfs_bindex_t bindex)
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
