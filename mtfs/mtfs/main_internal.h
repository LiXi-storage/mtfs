/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_MAIN_INTERNAL_H__
#define __MTFS_MAIN_INTERNAL_H__
#include <linux/fs.h>

extern struct proc_dir_entry *mtfs_proc_root;
extern struct proc_dir_entry *mtfs_proc_device;

void mtfs_reserve_fini(struct super_block *sb);
#endif /* __MTFS_MAIN_INTERNAL_H__ */
