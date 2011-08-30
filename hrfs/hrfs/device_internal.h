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
struct hrfs_dev_branch {
	int length;
	char *path;
	struct lowerfs_operations *ops;
};
struct hrfs_device {
	struct list_head device_list;
	char *device_name;
	int name_length;
	struct super_block *sb;
	struct hrfs_junction *junction;
	hrfs_bindex_t bnum;
	struct hrfs_dev_branch *branch;
};

#define hrfs_dev2bops(device, bindex) (device->branch[bindex].ops)
#define hrfs_dev2bpath(device, bindex) (device->branch[bindex].path)
#define hrfs_dev2blength(device, bindex) (device->branch[bindex].length)
#define hrfs_dev2bnum(device) (device->bnum)
#define hrfs_dev2junction(device) (device->junction)
#define hrfs_dev2ops(device) (hrfs_dev2junction(device)->fs_ops)

struct hrfs_device *hrfs_newdev(struct super_block *sb, mount_option_t *mount_option);
void hrfs_freedev(struct hrfs_device *device);
int hrfs_proc_read_devices(char *page, char **start, off_t off,
                           int count, int *eof, void *data);
#endif /* __HRFS_DEVICE_INTERNAL_H__ */
