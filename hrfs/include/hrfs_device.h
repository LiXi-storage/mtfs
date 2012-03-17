/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __HRFS_DEVICE_H__
#define __HRFS_DEVICE_H__
#if defined (__linux__) && defined(__KERNEL__)
#include <linux/list.h>
#include <parse_option.h>
#include <hrfs_common.h>
#include <hrfs_junction.h>

struct hrfs_branch_debug {
	int active; /* Wheather debug is active */
	int errno;  /* Every write operation should return this errno */
	__u32 bops_emask; /* Branch operation error mask */
};

struct hrfs_device_branch {
	char *path;                          /* Path for each branch */
	int path_length;                     /* Length of the path for each branch */
	struct lowerfs_operations *ops;      /* Lowerfs operation for each branch*/
	struct hrfs_branch_debug debug;      /* Debug option for each branch */
	struct proc_dir_entry *proc_entry;   /* Proc entry for each branch */
};

struct hrfs_device {
	struct list_head device_list;        /* Managed in the device list */
	char *device_name;                   /* Name of the device */
	int name_length;                     /* Length of the device name */
	struct super_block *sb;              /* Super block this device belong to */
	struct hrfs_junction *junction;      /* Junction to lowerfs */
	struct proc_dir_entry *proc_entry;   /* Proc entry for this device */
	int no_abort;                        /* Do not abort when no latest branch success */
	hrfs_bindex_t bnum;                  /* Branch number */
	struct hrfs_device_branch *branch;   /* Info for each branch */
};

#define hrfs_dev2bops(device, bindex) (device->branch[bindex].ops)
#define hrfs_dev2bpath(device, bindex) (device->branch[bindex].path)
#define hrfs_dev2blength(device, bindex) (device->branch[bindex].path_length)
#define hrfs_dev2bproc(device, bindex) (device->branch[bindex].proc_entry)
#define hrfs_dev2branch(device, bindex) (&device->branch[bindex])
#define hrfs_dev2bnum(device) (device->bnum)
#define hrfs_dev2junction(device) (device->junction)
#define hrfs_dev2ops(device) (hrfs_dev2junction(device)->fs_ops)

#include <hrfs_super.h>
static inline struct hrfs_device *hrfs_i2dev(struct inode *inode)
{
	struct super_block *sb = inode->i_sb;
	return hrfs_s2dev(sb);
}

static inline struct hrfs_device *hrfs_d2dev(struct dentry *dentry)
{
	struct super_block *sb = dentry->d_sb;
	return hrfs_s2dev(sb);
}

static inline struct hrfs_device *hrfs_f2dev(struct file *file)
{
	return hrfs_d2dev(file->f_dentry);
}

#define BOPS_MASK_WRITE 0x00000001
#define BOPS_MASK_READ  0x00000002

extern int hrfs_device_branch_errno(struct hrfs_device *device, hrfs_bindex_t bindex, __u32 emask);
#endif /* defined (__linux__) && defined(__KERNEL__) */
#endif /* __HRFS_DEVICE_H__ */
