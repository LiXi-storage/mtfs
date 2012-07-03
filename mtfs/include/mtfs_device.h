/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_DEVICE_H__
#define __MTFS_DEVICE_H__
#if defined (__linux__) && defined(__KERNEL__)
#include <linux/list.h>
#include <parse_option.h>
#include <mtfs_common.h>
#include <mtfs_junction.h>

struct mtfs_branch_debug {
	int active; /* Wheather debug is active */
	int errno;  /* Every write operation should return this errno */
	__u32 bops_emask; /* Branch operation error mask */
};

struct mtfs_device_branch {
	char *path;                          /* Path for each branch */
	int path_length;                     /* Length of the path for each branch */
	struct lowerfs_operations *ops;      /* Lowerfs operation for each branch*/
	struct mtfs_branch_debug debug;      /* Debug option for each branch */
	struct proc_dir_entry *proc_entry;   /* Proc entry for each branch */
};

struct mtfs_device {
	struct list_head device_list;        /* Managed in the device list */
	char *device_name;                   /* Name of the device */
	int name_length;                     /* Length of the device name */
	struct super_block *sb;              /* Super block this device belong to */
	struct mtfs_junction *junction;      /* Junction to lowerfs */
	struct proc_dir_entry *proc_entry;   /* Proc entry for this device */
	int no_abort;                        /* Do not abort when no latest branch success */
	mtfs_bindex_t bnum;                  /* Branch number */
	struct mtfs_device_branch branch[MTFS_BRANCH_MAX];   /* Info for each branch */
};

#define mtfs_dev2bops(device, bindex) (device->branch[bindex].ops)
#define mtfs_dev2bpath(device, bindex) (device->branch[bindex].path)
#define mtfs_dev2blength(device, bindex) (device->branch[bindex].path_length)
#define mtfs_dev2bproc(device, bindex) (device->branch[bindex].proc_entry)
#define mtfs_dev2branch(device, bindex) (&device->branch[bindex])
#define mtfs_dev2bnum(device) (device->bnum)
#define mtfs_dev2junction(device) (device->junction)
#define mtfs_dev2ops(device) (mtfs_dev2junction(device)->fs_ops)

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
