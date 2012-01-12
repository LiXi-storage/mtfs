/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
 */

#ifndef __HRFS_MAIN_INTERNAL_H__
#define __HRFS_MAIN_INTERNAL_H__

extern struct kmem_cache *hrfs_file_info_cache;
extern struct kmem_cache *hrfs_dentry_info_cache;
extern struct kmem_cache *hrfs_inode_info_cache;
extern struct kmem_cache *hrfs_sb_info_cache;
extern struct kmem_cache *hrfs_device_cache;

extern int hrfs_debug_level;
#endif /* __HRFS_MAIN_H__ */
