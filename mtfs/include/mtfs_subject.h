/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_SUBJECT_H__
#define __MTFS_SUBJECT_H__

#if defined(__linux__) && defined(__KERNEL__)
#include <linux/fs.h>

struct mtfs_subject_operations {
	int (*mso_init)(struct super_block *sb);
	int (*mso_fini)(struct super_block *sb);
};

#else /* !defined (__linux__) && defined(__KERNEL__) */
#error This head is only for kernel space use
#endif /* !defined (__linux__) && defined(__KERNEL__) */

#endif /* __MTFS_SUBJECT_H__ */
