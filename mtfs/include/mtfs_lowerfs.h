/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_LOWERFS_H__
#define __MTFS_LOWERFS_H__

#if defined (__linux__) && defined(__KERNEL__)

#include <linux/list.h>
#include <linux/fs.h>

#define MLOWERFS_FLAG_RMDIR_NO_DDLETE  0x00000001 /* When rmdir lowerfs, avoid d_delete in vfs_rmdir */
#define MLOWERFS_FLAG_UNLINK_NO_DDLETE 0x00000002 /* When unlink lowerfs, avoid d_delete in vfs_rmdir */
#define MLOWERFS_FLAG_ALL (MLOWERFS_FLAG_RMDIR_NO_DDLETE | MLOWERFS_FLAG_UNLINK_NO_DDLETE)

static inline int mml_flag_is_valid(unsigned long ml_flag)
{
	if (ml_flag & (~MLOWERFS_FLAG_ALL)) {
		return 0;
	}
	return 1;
}

struct mtfs_lowerfs {
	struct list_head ml_linkage;
	struct module   *ml_owner;
	const char      *ml_type;
	unsigned long    ml_magic; /* The same with sb->s_magic */
	unsigned long    ml_flag;

	/* Operations that should be provided */
	int (* ml_setflag)(struct inode *inode, __u32 flag);
	int (* ml_getflag)(struct inode *inode, __u32 *mtfs_flag);
};

extern int mlowerfs_register(struct mtfs_lowerfs *fs_ops);
extern void mlowerfs_unregister(struct mtfs_lowerfs *fs_ops);
extern int mlowerfs_getflag_xattr(struct inode *inode, __u32 *mtfs_flag, const char *xattr_name);
extern int mlowerfs_setflag_xattr(struct inode *inode, __u32 mtfs_flag, const char *xattr_name);
extern int mlowerfs_getflag_default(struct inode *inode, __u32 *mtfs_flag);
extern int mlowerfs_setflag_default(struct inode *inode, __u32 mtfs_flag);
#else /* !defined (__linux__) && defined(__KERNEL__) */
#error This head is only for kernel space use
#endif /* !defined (__linux__) && defined(__KERNEL__) */

#endif /* __MTFS_LOWERFS_H__ */
