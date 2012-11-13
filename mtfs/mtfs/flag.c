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
	struct mtfs_lowerfs *lowerfs = NULL;
	struct inode *hidden_inode = NULL;

	lowerfs = mtfs_i2bops(inode, bindex);
	hidden_inode = mtfs_i2branch(inode, bindex);
	MASSERT(lowerfs);
	MASSERT(hidden_inode);
	ret = mlowerfs_invalidate_data(lowerfs, hidden_inode);

	return ret;	
}

int mtfs_flag_invalidate_attr(struct inode *inode, mtfs_bindex_t bindex)
{
	return 0;
}

int mtfs_get_branch_flag(struct inode *inode, mtfs_bindex_t bindex, __u32 *mtfs_flag)
{
	struct inode *hidden_inode = mtfs_i2branch(inode, bindex);
	struct mtfs_lowerfs *lowerfs = mtfs_i2bops(inode, bindex);
	int ret = 0;
	MENTRY();

	MASSERT(hidden_inode);
	if (lowerfs->ml_getflag) {
		ret = lowerfs->ml_getflag(hidden_inode, mtfs_flag);
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
	struct mtfs_lowerfs *lowerfs = mtfs_i2bops(inode, bindex);
	int ret = 0;
	MENTRY();

	MASSERT(hidden_inode);
	if (lowerfs->ml_getflag == NULL) {
		MERROR("get flag not support\n");
		goto out;
	}

	if (lowerfs->ml_setflag == NULL) {
		MERROR("set flag not support\n");
		goto out;
	}

	ret = lowerfs->ml_getflag(hidden_inode, &mtfs_flag);
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
	ret = lowerfs->ml_setflag(hidden_inode, mtfs_flag);	
	if (ret) {
		MERROR("set flag failed, ret %d\n", ret);
		goto out;
	}
out:
	MRETURN(ret);
}
EXPORT_SYMBOL(mtfs_invalid_branch);
