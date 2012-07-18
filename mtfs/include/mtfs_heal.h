/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_HEAL_H__
#define __MTFS_HEAL_H__

#if defined(__linux__) && defined(__KERNEL__)
#include <linux/fs.h>
#include "mtfs_oplist.h"

struct heal_operations {
	int (*ho_discard_dentry)(struct inode *dir,
	                        struct dentry *dentry,
	                        struct mtfs_operation_list *list);
};

extern int heal_discard_dentry_sync(struct inode *dir,
                                    struct dentry *dentry, struct mtfs_operation_list *list);
extern int heal_discard_dentry_async(struct inode *dir,
                                     struct dentry *dentry, struct mtfs_operation_list *list);
#else /* !defined (__linux__) && defined(__KERNEL__) */
#error This head is only for kernel space use
#endif /* !defined (__linux__) && defined(__KERNEL__) */

#endif /* __MTFS_HEAL_H__ */
