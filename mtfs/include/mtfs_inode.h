/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __HRFS_INODE_H__
#define __HRFS_INODE_H__
#if defined (__linux__) && defined(__KERNEL__)
#include "debug.h"
#include <linux/fs.h>
#include <linux/namei.h>
#include "mtfs_common.h"

struct mtfs_inode_branch {
	struct inode *binode;
};

/* mtfs inode data in memory */
struct mtfs_inode_info {
	struct inode vfs_inode;
	struct rw_semaphore rwsem; /* protect barray */
	struct semaphore write_sem;
	mtfs_bindex_t bnum;
	struct mtfs_inode_branch barray[HRFS_BRANCH_MAX];
};

/* DO NOT access mtfs_*_info_t directly, use following macros */
#define mtfs_i2info(inode) (container_of(inode, struct mtfs_inode_info, vfs_inode))
#define mtfs_i2bnum(inode) (mtfs_i2info(inode)->bnum)
#define mtfs_i2barray(inode) (mtfs_i2info(inode)->barray)
#define mtfs_i2branch(inode, bindex) (mtfs_i2barray(inode)[bindex].binode)
#define mtfs_i2rwsem(inode) (mtfs_i2info(inode)->rwsem)
#define mtfs_i2wsem(inode) (mtfs_i2info(inode)->write_sem)

static inline void mtfs_i_init_lock(struct inode *inode)
{
	init_rwsem(&mtfs_i2rwsem(inode));
}

static inline int mtfs_i_is_locked(struct inode *inode)
{
	return !down_write_trylock(&mtfs_i2rwsem(inode));
}

static inline int mtfs_i_is_write_locked(struct inode *inode)
{
	return !down_read_trylock(&mtfs_i2rwsem(inode));
}

static inline void mtfs_i_read_lock(struct inode *inode)
{
	down_read(&mtfs_i2rwsem(inode));
}

static inline void mtfs_i_read_unlock(struct inode *inode)
{
	up_read(&mtfs_i2rwsem(inode));
}

static inline void mtfs_i_write_lock(struct inode *inode)
{
	down_write(&mtfs_i2rwsem(inode));
}

static inline void mtfs_i_write_unlock(struct inode *inode)
{
	up_write(&mtfs_i2rwsem(inode));
}

static inline void mtfs_i_wlock_init(struct inode *inode)
{
	sema_init(&mtfs_i2wsem(inode), 1);
}

static inline int mtfs_i_wlock_down_interruptible(struct inode *inode)
{
	return down_interruptible(&mtfs_i2wsem(inode));
}

static inline void mtfs_i_wlock_up(struct inode *inode)
{
	up(&mtfs_i2wsem(inode));
}
/* The values for mtfs_interpose's flag. */
#define INTERPOSE_DEFAULT	0
#define INTERPOSE_LOOKUP	1
#define INTERPOSE_SUPER		2

#define inode_is_locked(inode) (mutex_is_locked(&(inode)->i_mutex))

extern struct dentry *mtfs_lookup(struct inode *dir, struct dentry *dentry, struct nameidata *nd);
extern int mtfs_create(struct inode *dir, struct dentry *dentry, int mode, struct nameidata *nd);
extern int mtfs_link(struct dentry *old_dentry, struct inode *dir, struct dentry *new_dentry);
extern int mtfs_unlink(struct inode *dir, struct dentry *dentry);
extern int mtfs_rmdir(struct inode *dir, struct dentry *dentry);
extern int mtfs_symlink(struct inode *dir, struct dentry *dentry, const char *symname);
extern int mtfs_mkdir(struct inode *dir, struct dentry *dentry, int mode);
extern int mtfs_mknod(struct inode *dir, struct dentry *dentry, int mode, dev_t dev);
extern int mtfs_rename(struct inode *old_dir, struct dentry *old_dentry, struct inode *new_dir, struct dentry *new_dentry);
extern int mtfs_readlink(struct dentry *dentry, char __user *buf, int bufsiz);
void mtfs_put_link(struct dentry *dentry, struct nameidata *nd, void *ptr);
extern void *mtfs_follow_link(struct dentry *dentry, struct nameidata *nd);
extern int mtfs_permission(struct inode *inode, int mask, struct nameidata *nd);
extern int mtfs_setattr(struct dentry *dentry, struct iattr *ia);
extern int mtfs_getattr(struct vfsmount *mnt, struct dentry *dentry, struct kstat *stat);
extern ssize_t mtfs_getxattr(struct dentry *dentry, const char *name, void *value, size_t size);
extern int mtfs_setxattr(struct dentry *dentry, const char *name, const void *value, size_t size, int flags);
extern int mtfs_removexattr(struct dentry *dentry, const char *name);
extern ssize_t mtfs_listxattr(struct dentry *dentry, char *list, size_t size);
extern int mtfs_lookup_backend(struct inode *dir, struct dentry *dentry, int interpose_flag);
extern int mtfs_update_inode_size(struct inode *inode);
extern int mtfs_update_inode_attr(struct inode *inode);

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
#endif /* defined (__linux__) && defined(__KERNEL__) */
#endif /* __HRFS_INODE_H__ */
