/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <linux/fs.h>
#include <linux/module.h>
#include <mtfs_common.h>
#include "lowerfs_internal.h"

int mtfs_branch_getflag(struct inode *inode, mtfs_bindex_t bindex, __u32 *mtfs_flag)
{
	struct inode *hidden_inode = mtfs_i2branch(inode, bindex);
	struct mtfs_lowerfs *lowerfs = mtfs_i2blowerfs(inode, bindex);
	int ret = 0;
	MENTRY();

	MASSERT(hidden_inode);
	MASSERT(lowerfs);

	ret = mlowerfs_getflag(lowerfs, hidden_inode, mtfs_flag);
	MRETURN(ret);
}

int mtfs_branch_valid_flag(struct inode *inode, mtfs_bindex_t bindex, __u32 valid_flags)
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

	ret = mtfs_branch_getflag(inode, bindex, &mtfs_flag);
	if (ret) {
		MERROR("failed to get branch flag, ret = %d\n", ret);
		is_valid = 0;
		goto out;
	}

	if ((valid_flags & MTFS_DATA_VALID) != 0 &&
	    (mtfs_flag & MTFS_FLAG_DATABAD) != 0) {
		MDEBUG("data of branch[%d] is not valid\n", bindex); 
		is_valid = 0;
	}

out:
	MRETURN(is_valid);
}

int mtfs_branch_valid(struct inode *inode, mtfs_bindex_t bindex, __u32 valid_flags)
{
	int is_valid = 1;
	MENTRY();

	is_valid = mtfs_branch_valid_flag(inode, bindex, valid_flags);
	MRETURN(is_valid);
}
EXPORT_SYMBOL(mtfs_branch_valid);

int mtfs_branch_invalidate_flag(struct inode *inode, mtfs_bindex_t bindex, __u32 valid_flags)
{
	struct inode *hidden_inode = mtfs_i2branch(inode, bindex);
	struct mtfs_lowerfs *lowerfs = NULL;
	int ret = 0;
	MENTRY();

	MASSERT(hidden_inode);
	lowerfs = mtfs_i2blowerfs(inode, bindex);
	MASSERT(lowerfs);

	ret = mlowerfs_invalidate(lowerfs, hidden_inode, valid_flags);
	MRETURN(ret);
}

int mtfs_branch_invalidate(struct inode *inode, mtfs_bindex_t bindex, __u32 valid_flags)
{
	int ret = 0;
	MENTRY();

	ret = mtfs_branch_invalidate_flag(inode, bindex, valid_flags);
	MRETURN(ret);
}
EXPORT_SYMBOL(mtfs_branch_invalidate);
