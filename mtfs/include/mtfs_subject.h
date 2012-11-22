/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_SUBJECT_H__
#define __MTFS_SUBJECT_H__

#if defined(__linux__) && defined(__KERNEL__)
#include <linux/fs.h>

struct mtfs_subject_operations {
	int (*mso_super_init)(struct super_block *sb);
	int (*mso_super_fini)(struct super_block *sb);
	int (*mso_inode_init)(struct inode *inode);
	int (*mso_inode_fini)(struct inode *inode);
};

struct mtfs_subject {
	struct list_head        ms_linkage;
	struct module          *ms_owner;
	const char             *ms_name;
};

extern int msubject_register(struct mtfs_subject *subject);
extern void msubject_unregister(struct mtfs_subject *subject);
#else /* !defined (__linux__) && defined(__KERNEL__) */
#error This head is only for kernel space use
#endif /* !defined (__linux__) && defined(__KERNEL__) */

#endif /* __MTFS_SUBJECT_H__ */
