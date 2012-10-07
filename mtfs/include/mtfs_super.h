/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_SUPER_H__
#define __MTFS_SUPER_H__

#if defined (__linux__) && defined(__KERNEL__)

#include <linux/fs.h>
#include <mtfs_common.h>

struct mtfs_sb_branch {
	struct super_block *msb_sb;
	struct vfsmount    *msb_mnt;
	struct dentry      *msb_drecover;
	struct dentry      *msb_dreserve;
};

/* mtfs super-block data in memory */
struct mtfs_sb_info {
	struct mtfs_device   *msi_device;
	mtfs_bindex_t         msi_bnum; /* branch number */
	struct mtfs_sb_branch msi_barray[MTFS_BRANCH_MAX];
};

/* DO NOT access mtfs_*_info_t directly, use following macros */
#define _mtfs_s2info(sb)             ((sb)->s_fs_info)
#define mtfs_s2info(sb)              ((struct mtfs_sb_info *)_mtfs_s2info(sb))

#define mtfs_s2bnum(sb)              (mtfs_s2info(sb)->msi_bnum)
#define mtfs_s2dev(sb)               (mtfs_s2info(sb)->msi_device)
#define mtfs_s2barray(sb)            (mtfs_s2info(sb)->msi_barray)

#define mtfs_s2branch(sb, bindex)    (mtfs_s2barray(sb)[bindex].msb_sb)
#define mtfs_s2mntbranch(sb, bindex) (mtfs_s2barray(sb)[bindex].msb_mnt)
#define mtfs_s2bdrecover(sb, bindex) (mtfs_s2barray(sb)[bindex].msb_drecover)
#define mtfs_s2bdreserve(sb, bindex) (mtfs_s2barray(sb)[bindex].msb_dreserve)

extern struct inode *mtfs_alloc_inode(struct super_block *sb);
extern void mtfs_destroy_inode(struct inode *inode);
extern void mtfs_put_super(struct super_block *sb);
extern int mtfs_statfs(struct dentry *dentry, struct kstatfs *buf);
extern void mtfs_clear_inode(struct inode *inode);
extern int mtfs_show_options(struct seq_file *m, struct vfsmount *mnt);

#else /* !defined (__linux__) && defined(__KERNEL__) */
#error This head is only for kernel space use
#endif /* !defined (__linux__) && defined(__KERNEL__) */

#endif /* __MTFS_SUPER_H__ */
