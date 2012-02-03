/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
 */

#ifndef __HRFS_SUPPORT_H__
#define __HRFS_SUPPORT_H__

#if defined (__linux__) && defined(__KERNEL__)
#include <linux/list.h>
#include <linux/fs.h>

#define LF_RMDIR_NO_DDLETE 0x00000001 /* When rmdir lowerfs, avoid d_delete in vfs_rmdir */
#define LF_UNLINK_NO_DDLETE 0x00000002 /* When rmdir lowerfs, avoid d_delete in vfs_rmdir */
struct lowerfs_operations {
	struct list_head lowerfs_list;
	struct module *lowerfs_owner;
	char *lowerfs_type;
	unsigned long lowerfs_magic; /* The same with sb->s_magic */
	unsigned long lowerfs_flag;

	/* Operations that should be provided */
	int (* lowerfs_inode_set_flag)(struct inode *inode, __u32 flag);
	int (* lowerfs_inode_get_flag)(struct inode *inode, __u32 *hrfs_flag);
	int (* lowerfs_idata_init)(struct inode *inode, struct inode *parent_inode, int is_primary);
	int (* lowerfs_idata_finit)(struct inode *inode);
};

extern int lowerfs_register_ops(struct lowerfs_operations *fs_ops);
extern void lowerfs_unregister_ops(struct lowerfs_operations *fs_ops);
#endif /* defined (__linux__) && defined(__KERNEL__) */
#endif /* __HRFS_SUPPORT_H__ */
