/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
 */

#ifndef __HRFS_DEVICE_INTERNAL_H__
#define __HRFS_DEVICE_INTERNAL_H__
#include <linux/list.h>
#include <parse_option.h>
#include <hrfs_common.h>
#include <hrfs_junction.h>
#define MAX_DEVICE_NAME 1024
extern spinlock_t hrfs_device_lock;

struct hrfs_branch_debug {
	int active; /* Wheather debug is active */
	int errno;  /* Every operation should return this errno */
};

struct hrfs_device_branch {
	int length;
	char *path;
	struct lowerfs_operations *ops;
	struct hrfs_branch_debug debug; /* For branch debug */
	struct proc_dir_entry *proc_entry;
};
struct hrfs_device {
	struct list_head device_list;
	char *device_name;
	int name_length;
	struct super_block *sb;
	struct hrfs_junction *junction;
	struct proc_dir_entry *proc_entry;
	hrfs_bindex_t bnum;
	struct hrfs_device_branch *branch;
};

#define hrfs_dev2bops(device, bindex) (device->branch[bindex].ops)
#define hrfs_dev2bpath(device, bindex) (device->branch[bindex].path)
#define hrfs_dev2blength(device, bindex) (device->branch[bindex].length)
#define hrfs_dev2bproc(device, bindex) (device->branch[bindex].proc_entry)
#define hrfs_dev2branch(device, bindex) (&device->branch[bindex])
#define hrfs_dev2bnum(device) (device->bnum)
#define hrfs_dev2junction(device) (device->junction)
#define hrfs_dev2ops(device) (hrfs_dev2junction(device)->fs_ops)

#include "super_internal.h"
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

struct hrfs_device *hrfs_newdev(struct super_block *sb, mount_option_t *mount_option);
void hrfs_freedev(struct hrfs_device *device);
int hrfs_proc_read_devices(char *page, char **start, off_t off,
                           int count, int *eof, void *data);
int hrfs_device_branch_errno(struct hrfs_device *device, hrfs_bindex_t bindex);
#endif /* __HRFS_DEVICE_INTERNAL_H__ */
