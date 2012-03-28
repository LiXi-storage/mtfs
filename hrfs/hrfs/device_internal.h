/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __HRFS_DEVICE_INTERNAL_H__
#define __HRFS_DEVICE_INTERNAL_H__
#include <linux/list.h>
#include <parse_option.h>
#include <hrfs_common.h>
#include <hrfs_junction.h>
#include <hrfs_device.h>
#include "super_internal.h"

extern spinlock_t hrfs_device_lock;

struct hrfs_device *hrfs_newdev(struct super_block *sb, mount_option_t *mount_option);
void hrfs_freedev(struct hrfs_device *device);
int hrfs_proc_read_devices(char *page, char **start, off_t off,
                           int count, int *eof, void *data);
#endif /* __HRFS_DEVICE_INTERNAL_H__ */
