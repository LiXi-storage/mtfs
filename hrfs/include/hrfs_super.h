/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __HRFS_SUPER_H__
#define __HRFS_SUPER_H__
#if defined (__linux__) && defined(__KERNEL__)
#include <hrfs_common.h>

struct hrfs_sb_branch {
	struct super_block *b_sb;
	struct vfsmount    *b_mnt;
	struct dentry      *b_drecover;
};

/* hrfs super-block data in memory */
struct hrfs_sb_info {
	struct        hrfs_device *device;
	hrfs_bindex_t bnum; /* branch number */
	struct        hrfs_sb_branch barray[HRFS_BRANCH_MAX];
};

/* DO NOT access hrfs_*_info_t directly, use following macros */
#define _hrfs_s2info(sb)             ((sb)->s_fs_info)
#define hrfs_s2info(sb)              ((struct hrfs_sb_info *)_hrfs_s2info(sb))
#define hrfs_s2barray(sb)            (hrfs_s2info(sb)->barray)
#define hrfs_s2branch(sb, bindex)    (hrfs_s2barray(sb)[bindex].b_sb)
#define hrfs_s2mntbranch(sb, bindex) (hrfs_s2barray(sb)[bindex].b_mnt)
#define hrfs_s2brecover(sb, bindex)  (hrfs_s2barray(sb)[bindex].b_drecover)
#define hrfs_s2bnum(sb)              (hrfs_s2info(sb)->bnum)
#define hrfs_s2dev(sb)               (hrfs_s2info(sb)->device)

extern struct inode *hrfs_alloc_inode(struct super_block *sb);
extern void hrfs_destroy_inode(struct inode *inode);
extern void hrfs_put_super(struct super_block *sb);
extern int hrfs_statfs(struct dentry *dentry, struct kstatfs *buf);
extern void hrfs_clear_inode(struct inode *inode);
extern int hrfs_show_options(struct seq_file *m, struct vfsmount *mnt);
#endif /* defined (__linux__) && defined(__KERNEL__) */
#endif /* __HRFS_SUPER_H__ */
