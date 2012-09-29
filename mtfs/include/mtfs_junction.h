/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_JUNCTION_H__
#define __MTFS_JUNCTION_H__

#if defined (__linux__) && defined(__KERNEL__)
#include <linux/list.h>
#include <linux/fs.h>
#include "mtfs_heal.h"

struct mtfs_operations {
	struct inode_operations *symlink_iops;
	struct inode_operations *dir_iops;
	struct inode_operations *main_iops;
	struct file_operations *main_fops;
	struct file_operations *dir_fops;
	struct super_operations *sops;
	struct dentry_operations *dops;
	struct address_space_operations *aops;
	struct vm_operations_struct *vm_ops;
	int (*ioctl)(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);
	struct heal_operations *heal_ops;
};

struct mtfs_junction {
	struct list_head junction_list;
	struct module *junction_owner;
	const char *junction_name;

	const char *mj_subject;
	const char *primary_type;
	const char **secondary_types;

	struct mtfs_operations *fs_ops;
};

extern int junction_register(struct mtfs_junction *junction);
extern void junction_unregister(struct mtfs_junction *junction);

#else /* !defined (__linux__) && defined(__KERNEL__) */
#error This head is only for kernel space use
#endif /* !defined (__linux__) && defined(__KERNEL__) */

#endif /* __MTFS_JUNCTION_H__ */
