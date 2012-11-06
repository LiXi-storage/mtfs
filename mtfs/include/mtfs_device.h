/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_DEVICE_H__
#define __MTFS_DEVICE_H__
#if defined (__linux__) && defined(__KERNEL__)
#include <mtfs_list.h>
#include <parse_option.h>
#include <mtfs_common.h>
#include <mtfs_junction.h>
#include <mtfs_heal.h>

struct mtfs_branch_debug {
	int   mbd_active;     /* Wheather debug is active */
	int   mbd_errno;      /* Every write operation should return this errno */
	__u32 mbd_bops_emask; /* Branch operation error mask */
};

struct mtfs_device_branch {
	char                      *mdb_path;       /* Path for each branch */
	int                        mdb_pathlen;    /* Length of the path for each branch */
	struct lowerfs_operations *mdb_ops;        /* Lowerfs operation for each branch*/
	struct mtfs_branch_debug   mdb_debug;      /* Debug option for each branch */
	struct proc_dir_entry     *mdb_proc_entry; /* Proc entry for each branch */
};

struct mtfs_device {
	mtfs_list_t               md_list;                    /* Managed in the device list */
	struct super_block       *md_sb;                      /* Super block this device belong to */
	char                     *md_name;                    /* Name of the device */
	int                       md_namelen;                 /* Length of the device name */
	struct mtfs_junction     *md_junction;                /* Junction to lowerfs */
	struct proc_dir_entry    *md_proc_entry;              /* Proc entry for this device */
	int                       md_no_abort;                /* Do not abort when no latest branch success */
	mtfs_bindex_t             md_bnum;                    /* Branch number */
	struct mtfs_device_branch md_branch[MTFS_BRANCH_MAX]; /* Info for each branch */
};

#define mtfs_dev2sb(device)              (device->md_sb)
#define mtfs_dev2name(device)            (device->md_name)
#define mtfs_dev2namelen(device)         (device->md_namelen)
#define mtfs_dev2junction(device)        (device->md_junction)
#define mtfs_dev2proc(device)            (device->md_proc_entry)
#define mtfs_dev2noabort(device)         (device->md_no_abort)
#define mtfs_dev2ops(device)             (mtfs_dev2junction(device)->mj_fs_ops)
#define mtfs_dev2bnum(device)            (device->md_bnum)
#define mtfs_dev2branch(device, bindex)  (&device->md_branch[bindex])
#define mtfs_dev2bops(device, bindex)    (device->md_branch[bindex].mdb_ops)
#define mtfs_dev2bpath(device, bindex)   (device->md_branch[bindex].mdb_path)
#define mtfs_dev2blength(device, bindex) (device->md_branch[bindex].mdb_pathlen)
#define mtfs_dev2bdebug(device, bindex)  (device->md_branch[bindex].mdb_debug)
#define mtfs_dev2bproc(device, bindex)   (device->md_branch[bindex].mdb_proc_entry)


#include <mtfs_super.h>
static inline struct mtfs_device *mtfs_i2dev(struct inode *inode)
{
	struct super_block *sb = inode->i_sb;
	return mtfs_s2dev(sb);
}

static inline struct mtfs_device *mtfs_d2dev(struct dentry *dentry)
{
	struct super_block *sb = dentry->d_sb;
	return mtfs_s2dev(sb);
}

static inline struct mtfs_device *mtfs_f2dev(struct file *file)
{
	return mtfs_d2dev(file->f_dentry);
}

#define BOPS_MASK_WRITE 0x00000001
#define BOPS_MASK_READ  0x00000002

extern int mtfs_device_branch_errno(struct mtfs_device *device, mtfs_bindex_t bindex, __u32 emask);

#else /* !defined (__linux__) && defined(__KERNEL__) */
#error This head is only for kernel space use
#endif /* !defined (__linux__) && defined(__KERNEL__) */

#endif /* __MTFS_DEVICE_H__ */
