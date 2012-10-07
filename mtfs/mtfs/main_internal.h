/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_MAIN_INTERNAL_H__
#define __MTFS_MAIN_INTERNAL_H__
#include <linux/fs.h>

extern struct kmem_cache *mtfs_file_info_cache;
extern struct kmem_cache *mtfs_dentry_info_cache;
extern struct kmem_cache *mtfs_inode_info_cache;
extern struct kmem_cache *mtfs_sb_info_cache;
extern struct kmem_cache *mtfs_device_cache;
extern struct kmem_cache *mtfs_oplist_cache;
extern struct kmem_cache *mtfs_lock_cache;
extern struct kmem_cache *mtfs_interval_cache;
extern struct kmem_cache *mtfs_io_cache;

extern struct proc_dir_entry *mtfs_proc_root;
extern struct proc_dir_entry *mtfs_proc_device;

void mtfs_reserve_fini(struct super_block *sb);
#endif /* __MTFS_MAIN_H__ */
