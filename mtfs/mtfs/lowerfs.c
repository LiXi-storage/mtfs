/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <linux/module.h>
#include <debug.h>
#include "lowerfs_internal.h"

spinlock_t mtfs_lowerfs_lock = SPIN_LOCK_UNLOCKED;
LIST_HEAD(ml_types);

static struct mtfs_lowerfs *_mlowerfs_search(const char *type)
{
	struct mtfs_lowerfs *found = NULL;
	struct list_head *p = NULL;
	MENTRY();

	list_for_each(p, &ml_types) {
		found = list_entry(p, struct mtfs_lowerfs, ml_linkage);
		if (strcmp(found->ml_type, type) == 0) {
			MRETURN(found);
		}
	}

	MRETURN(NULL);
}

static int mlowerfs_check(struct mtfs_lowerfs *lowerfs)
{
	int ret = 0;
	MENTRY();

	if (lowerfs->ml_owner == NULL ||
	    lowerfs->ml_type == NULL ||
	    lowerfs->ml_magic == 0 ||
	    !mml_flag_is_valid(lowerfs->ml_flag) ||
	    lowerfs->ml_setflag == NULL ||
	    lowerfs->ml_getflag == NULL) {
	    	ret = -EINVAL;
	}

	MRETURN(ret);
}

static int _mlowerfs_register(struct mtfs_lowerfs *fs_ops)
{
	struct mtfs_lowerfs *found = NULL;
	int ret = 0;
	MENTRY();

	ret = mlowerfs_check(fs_ops);
	if (ret){
		MERROR("lowerfs is not valid\n");
		goto out;
	}

	if ((found = _mlowerfs_search(fs_ops->ml_type))) {
		if (found != fs_ops) {
			MERROR("try to register multiple operations for type %s\n",
			        fs_ops->ml_type);
		} else {
			MERROR("operation of type %s has already been registered\n",
			        fs_ops->ml_type);
		}
		ret = -EALREADY;
	} else {
		//PORTAL_MODULE_USE;
		list_add(&fs_ops->ml_linkage, &ml_types);
	}

out:
	MRETURN(ret);
}

int mlowerfs_register(struct mtfs_lowerfs *fs_ops)
{
	int ret = 0;
	MENTRY();

	spin_lock(&mtfs_lowerfs_lock);
	ret = _mlowerfs_register(fs_ops);
	spin_unlock(&mtfs_lowerfs_lock);

	MRETURN(ret);
}
EXPORT_SYMBOL(mlowerfs_register);

static void _mlowerfs_unregister(struct mtfs_lowerfs *fs_ops)
{
	struct list_head *p = NULL;
	MENTRY();

	list_for_each(p, &ml_types) {
		struct mtfs_lowerfs *found;

		found = list_entry(p, typeof(*found), ml_linkage);
		if (found == fs_ops) {
			list_del(p);
			break;
		}
	}

	_MRETURN();
}

void mlowerfs_unregister(struct mtfs_lowerfs *fs_ops)
{
	MENTRY();

	spin_lock(&mtfs_lowerfs_lock);
	_mlowerfs_unregister(fs_ops);
	spin_unlock(&mtfs_lowerfs_lock);

	_MRETURN();
}
EXPORT_SYMBOL(mlowerfs_unregister);

static struct mtfs_lowerfs *_mlowerfs_get(const char *type)
{
	struct mtfs_lowerfs *found = NULL;
	MENTRY();

	spin_lock(&mtfs_lowerfs_lock);
	found = _mlowerfs_search(type);
	if (found == NULL) {
		MERROR("failed to found lowerfs support for type %s\n", type);
	} else {
		if (try_module_get(found->ml_owner) == 0) {
			MERROR("failed to get module for lowerfs support for type %s\n", type);
			found = ERR_PTR(-EOPNOTSUPP);
		}
	}	
	spin_unlock(&mtfs_lowerfs_lock);

	MRETURN(found);
}

struct mtfs_lowerfs *mlowerfs_get(const char *type)
{
	struct mtfs_lowerfs *found = NULL;
	char module_name[32];
	MENTRY();

	found = _mlowerfs_get(type);
	if (found == NULL) {
		if (strcmp(type, "ext2") == 0 ||
		    strcmp(type, "ext3") == 0 ||
		    strcmp(type, "ext4") == 0) {
			snprintf(module_name, sizeof(module_name) - 1, "mtfs_ext");
		} else {
			snprintf(module_name, sizeof(module_name) - 1, "mtfs_%s", type);
		}

		if (request_module(module_name) != 0) {
			found = ERR_PTR(-ENOENT);
			MERROR("failed to load module %s\n", module_name);
		} else {
			found = _mlowerfs_get(type);
			if (found == NULL || IS_ERR(found)) {
				MERROR("failed to get lowerfs support after loading module %s, "
				       "type = %s\n",
			               module_name, type);
				found = found == NULL ? ERR_PTR(-EOPNOTSUPP) : found;
			}
		}
	} else if (IS_ERR(found)) {
		MERROR("failed to get lowerfs support for type %s\n", type);
	}

	MRETURN(found);
}

void mlowerfs_put(struct mtfs_lowerfs *fs_ops)
{
	MENTRY();

	module_put(fs_ops->ml_owner);

	_MRETURN();
}

int mlowerfs_getflag_xattr(struct inode *inode, __u32 *mtfs_flag, const char *xattr_name)
{
	struct dentry de = { .d_inode = inode };
	int ret = 0;
	int need_unlock = 0;
	__u32 disk_flag = 0;
	MENTRY();

	MASSERT(inode);
	MASSERT(inode->i_op);
	MASSERT(inode->i_op->getxattr);

	if (!inode_is_locked(inode)) {
		mutex_lock(&inode->i_mutex);
		need_unlock = 1;
	}

	ret = inode->i_op->getxattr(&de, xattr_name,
                              &disk_flag, sizeof(disk_flag));
	if (ret == -ENODATA) {
		MDEBUG("not set\n");
		*mtfs_flag = 0;
		goto out_succeeded;
	} else {
		if (unlikely(ret != sizeof(disk_flag))) {
			if (ret >= 0) {
				MERROR("getxattr ret = %d, expect %ld\n", ret, sizeof(disk_flag));
				ret = -EINVAL;
			} else {
				MERROR("getxattr ret = %d\n", ret);
			}
			goto out;
		}
	}

	if (unlikely(!mtfs_disk_flag_is_valid(disk_flag))) {
		MERROR("disk_flag 0x%x is not valid\n", disk_flag);
		ret = -EINVAL;
		goto out;
	}
	*mtfs_flag = disk_flag & MTFS_FLAG_DISK_MASK;
	MDEBUG("ret = %d, mtfs_flag = 0x%x\n", ret, *mtfs_flag);
out_succeeded:
	ret = 0;
out:
	if (need_unlock) {
		mutex_unlock(&inode->i_mutex);
	}
	MRETURN(ret);
}
EXPORT_SYMBOL(mlowerfs_getflag_xattr);

int mlowerfs_getflag_default(struct inode *inode, __u32 *mtfs_flag)
{
	int ret = 0;
	ret = mlowerfs_getflag_xattr(inode, mtfs_flag, XATTR_NAME_MTFS_FLAG);
	MRETURN(ret);
}
EXPORT_SYMBOL(mlowerfs_getflag_default);

int mlowerfs_setflag_xattr(struct inode *inode, __u32 mtfs_flag, const char *xattr_name)
{
	int ret = 0;
	struct dentry de = { .d_inode = inode };
	int flag = 0; /* Should success all the time, so NO XATTR_CREATE or XATTR_REPLACE */
	__u32 disk_flag = 0;
	int need_unlock = 0;
	MENTRY();

	MASSERT(inode);
	if (!inode_is_locked(inode)) {
		mutex_lock(&inode->i_mutex);
		need_unlock = 1;
	}

	if(unlikely(!mtfs_flag_is_valid(mtfs_flag))) {
		MERROR("0x%x is not a valid mtfs_flag\n", mtfs_flag);
		ret = -EPERM;
		goto out;
	}

	disk_flag = mtfs_flag | MTFS_FLAG_DISK_SYMBOL;
	if (unlikely(!inode->i_op || !inode->i_op->setxattr)) {
		ret = -EPERM;
		goto out;
	}

	ret = inode->i_op->setxattr(&de, xattr_name,
                             &disk_flag, sizeof(disk_flag), flag);
	if (ret != 0) {
		MERROR("setxattr failed, rc = %d\n", ret);
		goto out;
	}

out:
	if (need_unlock) {
		mutex_unlock(&inode->i_mutex);
	}
	MRETURN(ret);
}
EXPORT_SYMBOL(mlowerfs_setflag_xattr);

int mlowerfs_setflag_default(struct inode *inode, __u32 mtfs_flag)
{
	int ret = 0;
	ret = mlowerfs_setflag_xattr(inode, mtfs_flag, XATTR_NAME_MTFS_FLAG);
	MRETURN(ret);
}
EXPORT_SYMBOL(mlowerfs_setflag_default);
