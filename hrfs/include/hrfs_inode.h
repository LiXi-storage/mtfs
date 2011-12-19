/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
 */

#ifndef __HRFS_INODE_H__
#define __HRFS_INODE_H__

#include <debug.h>
#include <linux/fs.h>
#include <linux/namei.h>

typedef struct hrfs_inode_branch {
	__u64 invalid_flags;
	struct inode *binode;
} hrfs_i_branch_t;

/* hrfs inode data in memory */
typedef struct hrfs_inode_info {
	struct inode vfs_inode;
	struct rw_semaphore rwsem; /* protect barray */
	hrfs_bindex_t bnum;
	hrfs_i_branch_t *barray; /*TODO: change to barray[] */
} hrfs_i_info_t;

/* DO NOT access hrfs_*_info_t directly, use following macros */
#define hrfs_i2info(inode) (container_of(inode, hrfs_i_info_t, vfs_inode))
#define hrfs_i2bnum(inode) (hrfs_i2info(inode)->bnum)
#define hrfs_i2barray(inode) (hrfs_i2info(inode)->barray)
#define hrfs_i2branch(inode, bindex) (hrfs_i2barray(inode)[bindex].binode)
#define hrfs_i2binvalid(inode, bindex) (hrfs_i2barray(inode)[bindex].invalid_flags)
#define hrfs_i2rwsem(inode) (hrfs_i2info(inode)->rwsem)

static inline void hrfs_i_init_lock(struct inode *inode)
{
	init_rwsem(&hrfs_i2rwsem(inode));
}

static inline int hrfs_i_is_locked(struct inode *inode)
{
	return !down_write_trylock(&hrfs_i2rwsem(inode));
}

static inline int hrfs_i_is_write_locked(struct inode *inode)
{
	return !down_read_trylock(&hrfs_i2rwsem(inode));
}

static inline void hrfs_i_read_lock(struct inode *inode)
{
	down_read(&hrfs_i2rwsem(inode));
}

static inline void hrfs_i_read_unlock(struct inode *inode)
{
	up_read(&hrfs_i2rwsem(inode));
}

static inline void hrfs_i_write_lock(struct inode *inode)
{
	down_write(&hrfs_i2rwsem(inode));
}

static inline void hrfs_i_write_unlock(struct inode *inode)
{
	up_write(&hrfs_i2rwsem(inode));
}

#define HRFS_DENTRY_BIT 0x00000001
#define HRFS_INODE_BIT  0x00000002
#define HRFS_ATTR_BIT   0x00000004
#define HRFS_DATA_BIT   0x00000008

#define HRFS_INVALIDATE_DATA   (HRFS_DATA_BIT)
#define HRFS_INVALIDATE_ATTR   (HRFS_ATTR_BIT)
#define HRFS_INVALIDATE_INODE  (HRFS_INODE_BIT | HRFS_ATTR_BIT | HRFS_DATA_BIT)
#define HRFS_INVALIDATE_DENTRY (HRFS_INODE_VALID | HRFS_DENTRY_BIT)

#define HRFS_DENTRY_VALID      (HRFS_DENTRY_BIT)
#define HRFS_INODE_VALID       (HRFS_DENTRY_VALID | HRFS_INODE_BIT)
#define HRFS_ATTR_VALID        (HRFS_INODE_VALID | HRFS_ATTR_BIT)
#define HRFS_DATA_VALID        (HRFS_INODE_VALID | HRFS_DATA_BIT)

static inline int hrfs_valid_flags_is_valid(__u64 valid_flags)
{
	switch(valid_flags) {
	case HRFS_DENTRY_VALID:
	case HRFS_INODE_VALID:
	case HRFS_ATTR_VALID:
	case HRFS_DATA_VALID:
		return 1;
	default:
		return 0;
	}
	return 0;
}

static inline int hrfs_invalidate_flags_is_valid(__u64 invalidate_flags)
{
	switch(invalidate_flags) {
	case HRFS_INVALIDATE_DATA:
	case HRFS_INVALIDATE_ATTR:
	case HRFS_INVALIDATE_INODE:
	case HRFS_INVALIDATE_DENTRY:
		return 1;
	default:
		return 0;
	}
	return 0;
}

static inline hrfs_bindex_t hrfs_i_choose_bindex(struct inode *inode, __u64 valid_flags)
{
	hrfs_bindex_t bindex = 0;
	hrfs_bindex_t ret = -1;
	
	HASSERT(inode);
	HASSERT(hrfs_valid_flags_is_valid(valid_flags));

	hrfs_i_read_lock(inode);
	for (bindex = 0; bindex < hrfs_i2bnum(inode); bindex++) {
		if ((hrfs_i2branch(inode, bindex) != NULL)
			&& ((hrfs_i2binvalid(inode, bindex) & valid_flags) == 0)) {
			HDEBUG("branch[%d] chosen, binvalid = %llx\n", bindex, hrfs_i2binvalid(inode, bindex));
			ret = bindex;
			break;
		}
	}
	hrfs_i_read_unlock(inode);

	return ret;	
}

static inline struct inode *hrfs_i_choose_branch(struct inode *inode, __u64 valid_flags)
{
	hrfs_bindex_t bindex = hrfs_i_choose_bindex(inode, valid_flags);
	struct inode *hidden_inode = NULL;

	HASSERT(bindex == 0); /* TODO: remove me */

	if (bindex >= 0 && bindex < hrfs_i2bnum(inode)) {
		hidden_inode = hrfs_i2branch(inode, bindex);
	}

	return hidden_inode;
}

static inline int hrfs_i_invalidate_branch(struct inode *inode, hrfs_bindex_t bindex, __u64 invalidate_flags)
{
	struct inode *hidden_inode = NULL;
	int ret = 0;

	HASSERT(inode);
	HASSERT(bindex >=0 && bindex < hrfs_i2bnum(inode));
	HASSERT(hrfs_invalidate_flags_is_valid(invalidate_flags));

	hrfs_i_write_lock(inode);
	hidden_inode = hrfs_i2branch(inode, bindex);
	if (invalidate_flags & HRFS_INODE_BIT) {
		if (hidden_inode != NULL) {
			HDEBUG("Setting a valid inode_branch's flag to invalid\n");
			hrfs_i2branch(inode, bindex) = NULL;
			//ret = -ENOMEM;
			//HBUG();
		}
	} else {
		if (hidden_inode == NULL) {
			HERROR("Setting a invalid inode_branch's flag\n");
			HBUG();
			ret = -ENOMEM;
			goto out;			
		}
	}
		
	hrfs_i2binvalid(inode, bindex) |= invalidate_flags;
out:
	hrfs_i_write_unlock(inode);
	return ret;
}

static inline int hrfs_i_branch_is_invalid(struct inode *inode, hrfs_bindex_t bindex, __u64 valid_flags)
{
	HASSERT(inode);
	//HASSERT(hrfs_i_is_locked(inode));
	HASSERT(bindex >=0 && bindex < hrfs_i2bnum(inode));
	HASSERT(hrfs_valid_flags_is_valid(valid_flags));

	return !!(hrfs_i2binvalid(inode, bindex) & valid_flags);
}

/* The values for hrfs_interpose's flag. */
#define INTERPOSE_DEFAULT	0
#define INTERPOSE_LOOKUP	1
#define INTERPOSE_SUPER		2

#define inode_is_locked(inode) (mutex_is_locked(&(inode)->i_mutex))

extern struct dentry *hrfs_lookup(struct inode *dir, struct dentry *dentry, struct nameidata *nd);
extern int hrfs_create(struct inode *dir, struct dentry *dentry, int mode, struct nameidata *nd);
extern int hrfs_link(struct dentry *old_dentry, struct inode *dir, struct dentry *new_dentry);
extern int hrfs_unlink(struct inode *dir, struct dentry *dentry);
extern int hrfs_rmdir(struct inode *dir, struct dentry *dentry);
extern int hrfs_symlink(struct inode *dir, struct dentry *dentry, const char *symname);
extern int hrfs_mkdir(struct inode *dir, struct dentry *dentry, int mode);
extern int hrfs_mknod(struct inode *dir, struct dentry *dentry, int mode, dev_t dev);
extern int hrfs_rename(struct inode *old_dir, struct dentry *old_dentry, struct inode *new_dir, struct dentry *new_dentry);
extern int hrfs_readlink(struct dentry *dentry, char __user *buf, int bufsiz);
void hrfs_put_link(struct dentry *dentry, struct nameidata *nd, void *ptr);
extern void *hrfs_follow_link(struct dentry *dentry, struct nameidata *nd);
extern int hrfs_permission(struct inode *inode, int mask, struct nameidata *nd);
extern int hrfs_setattr(struct dentry *dentry, struct iattr *ia);
extern int hrfs_getattr(struct vfsmount *mnt, struct dentry *dentry, struct kstat *stat);
extern ssize_t hrfs_getxattr(struct dentry *dentry, const char *name, void *value, size_t size);
extern int hrfs_setxattr(struct dentry *dentry, const char *name, const void *value, size_t size, int flags);
extern int hrfs_removexattr(struct dentry *dentry, const char *name);
extern ssize_t hrfs_listxattr(struct dentry *dentry, char *list, size_t size);
extern int hrfs_lookup_backend(struct inode *dir, struct dentry *dentry, int interpose_flag);
extern int hrfs_update_inode_size(struct inode *inode);
extern int hrfs_update_inode_attr(struct inode *inode);

static inline int hrfs_get_nlinks(struct inode *inode) 
{
	int sum_nlinks = 1;
	hrfs_bindex_t bindex = 0;
	struct inode *hidden_inode = NULL;

	for (bindex = 0; bindex < hrfs_i2bnum(inode); bindex++) {
		hidden_inode = hrfs_i2branch(inode, bindex);
	
		if (!hidden_inode) {
			continue;
		}
		sum_nlinks += (hidden_inode->i_nlink - 1);
	}
	return sum_nlinks;
}

static inline const char *hrfs_mode2type(umode_t mode)
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
#endif /* __HRFS_INODE_H__ */
