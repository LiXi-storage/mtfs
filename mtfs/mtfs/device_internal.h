/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_DEVICE_INTERNAL_H__
#define __MTFS_DEVICE_INTERNAL_H__
#include <linux/list.h>
#include <parse_option.h>
#include <mtfs_common.h>
#include <mtfs_junction.h>
#include <mtfs_device.h>
#include "super_internal.h"

extern spinlock_t mtfs_device_lock;

struct mtfs_device *mtfs_newdev(struct super_block *sb, struct mount_option *mount_option);
void mtfs_freedev(struct mtfs_device *device);
int mtfs_proc_read_devices(char *page, char **start, off_t off,
                           int count, int *eof, void *data);
#endif /* __MTFS_DEVICE_INTERNAL_H__ */
