/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <linux/fs.h>
#include <linux/module.h>
#include <mtfs_common.h>
#include "lowerfs_internal.h"

int mtfs_flag_invalidate_data(struct inode *inode, mtfs_bindex_t bindex)
{
	mtfs_bindex_t ret = -1;
	struct lowerfs_operations *lowerfs_ops = NULL;
	struct inode *hidden_inode = NULL;

	lowerfs_ops = mtfs_i2bops(inode, bindex);
	hidden_inode = mtfs_i2branch(inode, bindex);
	MASSERT(lowerfs_ops);
	MASSERT(hidden_inode);
	ret = lowerfs_inode_invalidate_data(lowerfs_ops, hidden_inode);

	return ret;	
}

int mtfs_flag_invalidate_attr(struct inode *inode, mtfs_bindex_t bindex)
{
	return 0;
}

int mtfs_get_branch_flag(struct inode *inode, mtfs_bindex_t bindex, __u32 *mtfs_flag)
{
	struct inode *hidden_inode = mtfs_i2branch(inode, bindex);
	struct lowerfs_operations *lowerfs_ops = mtfs_i2bops(inode, bindex);
	int ret = 0;
	MENTRY();

	MASSERT(hidden_inode);
	if (lowerfs_ops->lowerfs_inode_get_flag) {
		ret = lowerfs_ops->lowerfs_inode_get_flag(hidden_inode, mtfs_flag);
	}
	MRETURN(ret);
}

int mtfs_branch_is_valid(struct inode *inode, mtfs_bindex_t bindex, __u32 valid_flags)
{
	int is_valid = 1;
	__u32 mtfs_flag = 0;
	int ret = 0;
	MENTRY();

	if (mtfs_i2branch(inode, bindex) == NULL) {
		is_valid = 0;
		MDEBUG("branch[%d] is null, return invalid\n", bindex);
		goto out;
	}
	
	if (valid_flags == MTFS_BRANCH_VALID) {
		is_valid = 1;
		MDEBUG("branch[%d] is not null, return valid\n", bindex);
		goto out;
	}

	ret = mtfs_get_branch_flag(inode, bindex, &mtfs_flag);
	if (ret) {
		MERROR("failed to get branch flag, ret = %d\n", ret);
		is_valid = ret;
		goto out;
	}

	if ((valid_flags & MTFS_DATA_VALID) != 0 &&
	    (mtfs_flag & MTFS_FLAG_DATABAD) != 0) {
		MDEBUG("data of branch[%d] is not valid\n", bindex); 
		is_valid = 0;
	}
	/* TODO: xattr and attr */
out:
	MRETURN(is_valid);
}
EXPORT_SYMBOL(mtfs_branch_is_valid);

int mtfs_invalid_branch(struct inode *inode, mtfs_bindex_t bindex, __u32 valid_flags)
{
	__u32 mtfs_flag = 0;
	struct inode *hidden_inode = mtfs_i2branch(inode, bindex);
	struct lowerfs_operations *lowerfs_ops = mtfs_i2bops(inode, bindex);
	int ret = 0;
	MENTRY();

	MASSERT(hidden_inode);
	if (lowerfs_ops->lowerfs_inode_get_flag == NULL) {
		MERROR("get flag not support\n");
		goto out;
	}

	if (lowerfs_ops->lowerfs_inode_set_flag == NULL) {
		MERROR("set flag not support\n");
		goto out;
	}

	ret = lowerfs_ops->lowerfs_inode_get_flag(hidden_inode, &mtfs_flag);
	if (ret) {
		MERROR("get flag failed, ret %d\n", ret);
		goto out;
	}

	if ((valid_flags & MTFS_DATA_VALID) == 0) {
		MERROR("nothing to set\n");
		goto out;
	}

	if ((mtfs_flag & MTFS_FLAG_DATABAD) != 0) {
		MERROR("already set\n");
		goto out;
	}

	mtfs_flag |= MTFS_FLAG_DATABAD;
	mtfs_flag |= MTFS_FLAG_SETED;
	MDEBUG("mtfs_flag = %x\n", mtfs_flag);
	MASSERT(mtfs_flag_is_valid(mtfs_flag));
	ret = lowerfs_ops->lowerfs_inode_get_flag(hidden_inode, &mtfs_flag);	
	if (ret) {
		MERROR("set flag failed, ret %d\n", ret);
		goto out;
	}
out:
	MRETURN(ret);
}
EXPORT_SYMBOL(mtfs_invalid_branch);

int lowerfs_inode_get_flag_xattr(struct inode *inode, __u32 *mtfs_flag, const char *xattr_name)
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
EXPORT_SYMBOL(lowerfs_inode_get_flag_xattr);

int lowerfs_inode_get_flag_default(struct inode *inode, __u32 *mtfs_flag)
{
	int ret = 0;
	ret = lowerfs_inode_get_flag_xattr(inode, mtfs_flag, XATTR_NAME_MTFS_FLAG);
	MRETURN(ret);
}
EXPORT_SYMBOL(lowerfs_inode_get_flag_default);

int lowerfs_inode_set_flag_xattr(struct inode *inode, __u32 mtfs_flag, const char *xattr_name)
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
EXPORT_SYMBOL(lowerfs_inode_set_flag_xattr);

int lowerfs_inode_set_flag_default(struct inode *inode, __u32 mtfs_flag)
{
	int ret = 0;
	ret = lowerfs_inode_set_flag_xattr(inode, mtfs_flag, XATTR_NAME_MTFS_FLAG);
	MRETURN(ret);
}
EXPORT_SYMBOL(lowerfs_inode_set_flag_default);
