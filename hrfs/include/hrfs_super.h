/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
 */

#ifndef __HRFS_SUPER_H__
#define __HRFS_SUPER_H__
#if defined (__linux__) && defined(__KERNEL__)
#include <hrfs_common.h>

/* hrfs super-block data in memory */
typedef struct hrfs_sb_info {
	struct super_block **si_branch; /* hidden_super_block_array */
	struct vfsmount **si_mnt_branch; /* mnt_array */
	hrfs_bindex_t si_bnum; /* branch number */
	int si_level; /* raid level */
	struct dentry **si_d_recover;
	struct hrfs_device *si_device;
} hrfs_s_info_t;

/* DO NOT access hrfs_*_info_t directly, use following macros */
#define _hrfs_s2info(sb) ((sb)->s_fs_info)
#define hrfs_s2info(sb) ((hrfs_s_info_t *)_hrfs_s2info(sb))
#define hrfs_s2branch(sb, bindex) (hrfs_s2info(sb)->si_branch[bindex])
#define hrfs_s2mntbranch(sb, bindex) (hrfs_s2info(sb)->si_mnt_branch[bindex])
#define hrfs_s2bnum(sb) (hrfs_s2info(sb)->si_bnum)
#define hrfs_s2dev(sb) (hrfs_s2info(sb)->si_device)
#define hrfs_s2brecover(sb, bindex) (hrfs_s2info(sb)->si_d_recover[bindex])

extern struct inode *hrfs_alloc_inode(struct super_block *sb);
extern void hrfs_destroy_inode(struct inode *inode);
extern void hrfs_put_super(struct super_block *sb);
extern int hrfs_statfs(struct dentry *dentry, struct kstatfs *buf);
extern void hrfs_clear_inode(struct inode *inode);
extern int hrfs_show_options(struct seq_file *m, struct vfsmount *mnt);
#endif /* defined (__linux__) && defined(__KERNEL__) */
#endif /* __HRFS_SUPER_H__ */
