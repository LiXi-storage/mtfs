/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_INODE_H__
#define __MTFS_INODE_H__
#if defined (__linux__) && defined(__KERNEL__)

#include <linux/fs.h>
#include <linux/namei.h>
#include "debug.h"
#include "mtfs_async.h"
#include "mtfs_common.h"
#include "mtfs_lock.h"
#include "mtfs_lowerfs.h"

struct mtfs_inode_branch {
	struct inode           *mib_inode;
	struct mlowerfs_bucket  mib_bucket; /* Used by async_replica */
};

#define mlowerfs_bucket2branch(bucket) (container_of(bucket, struct mtfs_inode_branch, mib_bucket))
#define mlowerfs_bucket2inode(bucket)  (mlowerfs_bucket2branch(bucket)->mib_inode)

/* mtfs inode data in memory */
struct mtfs_inode_info {
	struct inode             mii_inode;
	struct mlock_resource    mii_resource; /* Used by mlock */
	struct masync_bucket     mii_bucket;   /* Used by async_replica */
	mtfs_bindex_t            mii_bnum;
	struct mtfs_inode_branch mii_barray[MTFS_BRANCH_MAX];
};

/* DO NOT access mtfs_*_info_t directly, use following macros */
#define mtfs_i2info(inode)            (container_of(inode, struct mtfs_inode_info, mii_inode))
#define mtfs_i2bnum(inode)            (mtfs_i2info(inode)->mii_bnum)
#define mtfs_i2barray(inode)          (mtfs_i2info(inode)->mii_barray)
#define mtfs_i2branch(inode, bindex)  (mtfs_i2barray(inode)[bindex].mib_inode)
#define mtfs_i2bbucket(inode, bindex) (&mtfs_i2barray(inode)[bindex].mib_bucket)
#define mtfs_i2resource(inode)        (&mtfs_i2info(inode)->mii_resource)
#define mtfs_i2bucket(inode)          (&mtfs_i2info(inode)->mii_bucket)
#define mtfs_bucket2info(bucket)      (container_of(bucket, struct mtfs_inode_info, mii_bucket))
#define mtfs_bucket2inode(bucket)     (&mtfs_bucket2info(bucket)->mii_inode)

/* The values for mtfs_interpose's flag. */
#define INTERPOSE_DEFAULT	0
#define INTERPOSE_LOOKUP	1
#define INTERPOSE_SUPER		2

#define inode_is_locked(inode) (mutex_is_locked(&(inode)->i_mutex))

extern struct dentry *mtfs_lookup(struct inode *dir, struct dentry *dentry, struct nameidata *nd);
struct dentry *mtfs_lookup_nonnd(struct inode *dir, struct dentry *dentry, struct nameidata *nd);
extern int mtfs_create(struct inode *dir, struct dentry *dentry, int mode, struct nameidata *nd);
extern int mtfs_link(struct dentry *old_dentry, struct inode *dir, struct dentry *new_dentry);
extern int mtfs_unlink(struct inode *dir, struct dentry *dentry);
extern int mtfs_rmdir(struct inode *dir, struct dentry *dentry);
extern int mtfs_symlink(struct inode *dir, struct dentry *dentry, const char *symname);
extern int mtfs_mkdir(struct inode *dir, struct dentry *dentry, int mode);
extern int mtfs_mknod(struct inode *dir, struct dentry *dentry, int mode, dev_t dev);
extern int mtfs_rename(struct inode *old_dir, struct dentry *old_dentry, struct inode *new_dir, struct dentry *new_dentry);
extern int mtfs_readlink(struct dentry *dentry, char __user *buf, int bufsiz);
extern void mtfs_put_link(struct dentry *dentry, struct nameidata *nd, void *ptr);
extern void *mtfs_follow_link(struct dentry *dentry, struct nameidata *nd);
#ifdef HAVE_INODE_PERMISION_2ARGS
extern int mtfs_permission(struct inode *inode, int mask);
#else /* ! HAVE_INODE_PERMISION_2ARGS */
extern int mtfs_permission(struct inode *inode, int mask, struct nameidata *nd);
#endif /* ! HAVE_INODE_PERMISION_2ARGS */
extern int mtfs_setattr(struct dentry *dentry, struct iattr *ia);
extern int mtfs_getattr(struct vfsmount *mnt, struct dentry *dentry, struct kstat *stat);
extern ssize_t mtfs_getxattr(struct dentry *dentry, const char *name, void *value, size_t size);
extern int mtfs_setxattr(struct dentry *dentry, const char *name, const void *value, size_t size, int flags);
extern int mtfs_removexattr(struct dentry *dentry, const char *name);
extern ssize_t mtfs_listxattr(struct dentry *dentry, char *list, size_t size);
extern int mtfs_lookup_backend(struct inode *dir, struct dentry *dentry, int interpose_flag);
extern int mtfs_update_attr_times(struct inode *inode);
extern int mtfs_update_attr_atime(struct inode *inode);
extern int mtfs_update_inode_attrs(struct inode *inode);
extern int mtfs_update_inode_size(struct inode *inode);
extern void mtfs_inode_size_dump(struct inode *inode);

static inline int mtfs_get_nlinks(struct inode *inode) 
{
	int sum_nlinks = 1;
	mtfs_bindex_t bindex = 0;
	struct inode *hidden_inode = NULL;

	for (bindex = 0; bindex < mtfs_i2bnum(inode); bindex++) {
		hidden_inode = mtfs_i2branch(inode, bindex);
	
		if (!hidden_inode) {
			continue;
		}
		sum_nlinks += (hidden_inode->i_nlink - 1);
	}
	return sum_nlinks;
}

static inline const char *mtfs_mode2type(umode_t mode)
{
	switch (mode & S_IFMT) {
	case S_IFLNK:  return "softlink"; break;
	case S_IFREG:  return "regular file"; break;
	case S_IFDIR:  return "directory"; break;
	case S_IFCHR:  return "char file"; break;
	case S_IFBLK:  return "block file"; break;
	case S_IFIFO:  return "fifo file"; break;
	case S_IFSOCK: return "socket file"; break;
	default:       return "unkown type file"; break;
	}
}

struct mtfs_iupdate_operations {
	int (*miuo_size)(struct inode *inode);
	int (*miuo_atime)(struct inode *inode);
	int (*miuo_times)(struct inode *inode);
	int (*miuo_attrs)(struct inode *inode);
};

extern struct mtfs_iupdate_operations mtfs_iupdate_choose;
extern struct mtfs_iupdate_operations mtfs_iupdate_master;

#else /* !defined (__linux__) && defined(__KERNEL__) */
#error This head is only for kernel space use
#endif /* !defined (__linux__) && defined(__KERNEL__) */

#endif /* __MTFS_INODE_H__ */
