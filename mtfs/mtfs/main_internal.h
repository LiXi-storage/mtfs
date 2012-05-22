/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __HRFS_MAIN_INTERNAL_H__
#define __HRFS_MAIN_INTERNAL_H__

extern struct kmem_cache *mtfs_file_info_cache;
extern struct kmem_cache *mtfs_dentry_info_cache;
extern struct kmem_cache *mtfs_inode_info_cache;
extern struct kmem_cache *mtfs_sb_info_cache;
extern struct kmem_cache *mtfs_device_cache;
extern struct kmem_cache *mtfs_oplist_cache;

extern int mtfs_debug_level;
#endif /* __HRFS_MAIN_H__ */
