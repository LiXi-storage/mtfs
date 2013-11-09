/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_SUPER_H__
#define __MTFS_SUPER_H__

#if defined (__linux__) && defined(__KERNEL__)

#include <linux/fs.h>
#include <mtfs_common.h>

#define MTFS_MAX_SUBJECT 64

/* On-disk configuration file. In host-endian order. */
struct mtfs_config_info {
	__u32         mci_magic;                     /* Should be MCI_MAGIC */
	__u64         mci_version;                   /* Rewrite count */
	mtfs_bindex_t mci_bindex;                    /* Branch index */
	mtfs_bindex_t mci_bnum;                      /* Branch number */
	char          mci_subject[MTFS_MAX_SUBJECT]; /* Subject name */
};

struct mtfs_config {
	/* Info is valid or not */
	__u32                   mc_valid;
	/* Branch is latest or not */
	int                     mc_blatest[MTFS_BRANCH_MAX];
	mtfs_bindex_t           mc_nonlatest;
	struct mtfs_config_info mc_info;
};

struct mtfs_sb_branch {
	struct super_block *msb_sb;
	struct vfsmount    *msb_mnt;
	struct dentry      *msb_dreserve;
	struct dentry      *msb_drecover;
	struct dentry      *msb_dconfig;
};

/* mtfs super-block data in memory */
struct mtfs_sb_info {
	struct mtfs_device   *msi_device;
	struct mtfs_config   *msi_config;
	void                 *msi_subject_info;
	mtfs_bindex_t         msi_bnum; /* branch number */
	struct mtfs_sb_branch msi_barray[MTFS_BRANCH_MAX];
};

/* DO NOT access mtfs_*_info_t directly, use following macros */
#define _mtfs_s2info(sb)             ((sb)->s_fs_info)
#define mtfs_s2info(sb)              ((struct mtfs_sb_info *)_mtfs_s2info(sb))

#define mtfs_s2bnum(sb)              (mtfs_s2info(sb)->msi_bnum)
#define mtfs_s2dev(sb)               (mtfs_s2info(sb)->msi_device)
#define mtfs_s2config(sb)            (mtfs_s2info(sb)->msi_config)
#define mtfs_s2subinfo(sb)           (mtfs_s2info(sb)->msi_subject_info)
#define mtfs_s2barray(sb)            (mtfs_s2info(sb)->msi_barray)

#define mtfs_s2branch(sb, bindex)    (mtfs_s2barray(sb)[bindex].msb_sb)
#define mtfs_s2mntbranch(sb, bindex) (mtfs_s2barray(sb)[bindex].msb_mnt)
#define mtfs_s2bdreserve(sb, bindex) (mtfs_s2barray(sb)[bindex].msb_dreserve)
#define mtfs_s2bdrecover(sb, bindex) (mtfs_s2barray(sb)[bindex].msb_drecover)
#define mtfs_s2bdconfig(sb, bindex)  (mtfs_s2barray(sb)[bindex].msb_dconfig)

extern struct inode *mtfs_alloc_inode(struct super_block *sb);
extern void mtfs_destroy_inode(struct inode *inode);
extern void mtfs_put_super(struct super_block *sb);
extern int mtfs_statfs(struct dentry *dentry, struct kstatfs *buf);
extern void mtfs_clear_inode(struct inode *inode);
extern int mtfs_show_options(struct seq_file *m, struct vfsmount *mnt);

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

#else /* !defined (__linux__) && defined(__KERNEL__) */
#error This head is only for kernel space use
#endif /* !defined (__linux__) && defined(__KERNEL__) */

#endif /* __MTFS_SUPER_H__ */
