/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_STACK_H__
#define __MTFS_STACK_H__

#if defined(__linux__) && defined(__KERNEL__)

#ifdef HAVE_FS_STACK
#include <linux/fs_stack.h>
#else /* !HAVE_FS_STACK */
/* COPY from linux-2.6.30/include/linux/fs_stack.h */

/* This file defines generic functions used primarily by stackable
 * filesystems; none of these functions require i_mutex to be held.
 */

#include <linux/fs.h>

/* externs for fs/stack.c */
extern void fsstack_copy_attr_all(struct inode *dest, const struct inode *src,
				int (*get_nlinks)(struct inode *));

extern void fsstack_copy_inode_size(struct inode *dst, const struct inode *src);

/* inlines */
static inline void fsstack_copy_attr_atime(struct inode *dest,
					   const struct inode *src)
{
	dest->i_atime = src->i_atime;
}

static inline void fsstack_copy_attr_times(struct inode *dest,
					   const struct inode *src)
{
	dest->i_atime = src->i_atime;
	dest->i_mtime = src->i_mtime;
	dest->i_ctime = src->i_ctime;
}

#endif /* !HAVE_FS_STACK */

#else /* !defined (__linux__) && defined(__KERNEL__) */
#error This head is only for kernel space use
#endif /* !defined (__linux__) && defined(__KERNEL__) */

#endif /* __MTFS_STACK_H__ */
