/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __HRFS_FLAG_H__
#define __HRFS_FLAG_H__
#include "mtfs_common.h"
#include "debug.h"
#include "mtfs_file.h"
/*
 * Same to macros defined in lustre/include/lustre_idl.h
 * We should keep this ture:
 * When flag is zero by default,
 * it means file data is proper and not under recovering.
 * Lowest 4 bits are for raid_pattern.
 */
#define HRFS_FLAG_PRIMARY                0x00000010
#define HRFS_FLAG_DATABAD                0x00000020
#define HRFS_FLAG_RECOVERING             0x00000040
#define HRFS_FLAG_SETED                  0x00000080

#define HRFS_FLAG_DISK_MASK              0x000000ff
#define HRFS_FLAG_DISK_SYMBOL            0xc0ffee00

#define HRFS_FLAG_RAID_MASK              0x0000000f

static inline int mtfs_flag_is_valid(__u32 mtfs_flag)
{
	int rc = 0;

	if (((mtfs_flag & HRFS_FLAG_SETED) == 0) &&
	    (mtfs_flag != 0)) {
		/* If not seted, then it is zero */
		goto out;
	}

	if ((mtfs_flag & (~HRFS_FLAG_DISK_MASK)) != 0) {
		/* Should not set unused flag*/
		goto out;
	}

	if ((mtfs_flag & HRFS_FLAG_RECOVERING) &&
	    (!(mtfs_flag & HRFS_FLAG_DATABAD))) {
		/* Branch being recovered should be bad*/
		goto out;
	}
	rc = 1;
out:
	return rc;
}

static inline int mtfs_disk_flag_is_valid(__u32 disk_flag)
{
	int ret = 0;
	__u32 mtfs_flag = disk_flag & HRFS_FLAG_DISK_MASK;

	if ((disk_flag & (~HRFS_FLAG_DISK_MASK)) != HRFS_FLAG_DISK_SYMBOL) {
		goto out;
	}

	ret = mtfs_flag_is_valid(mtfs_flag);
out:
	return ret;
}

#define HRFS_DATA_BIT     0x00001
#define HRFS_ATTR_BIT     0x00002
#define HRFS_XATTR_BIT    0x00004
#define HRFS_BRANCH_BIT    0x00008

#define HRFS_DATA_VALID   (HRFS_DATA_BIT)
#define HRFS_ATTR_VALID   (HRFS_ATTR_BIT)
#define HRFS_XATTR_VALID  (HRFS_XATTR_BIT)
#define HRFS_BRANCH_VALID (HRFS_BRANCH_BIT)
#define HRFS_ALL_VALID    (HRFS_DATA_BIT | HRFS_ATTR_BIT | HRFS_XATTR_BIT | HRFS_BRANCH_VALID)

static inline int mtfs_valid_flags_is_valid(__u32 valid_flags)
{
	int valid = 1;
	if ((valid_flags & (~HRFS_ALL_VALID)) != 0) {
		HERROR("valid_flags is not valid, unused bits is setted\n");
		valid = 0;
		goto out;
	}
out:
	return valid;
}

#define XATTR_NAME_HRFS_FLAG    "trusted.mtfs.flag"

#if defined (__linux__) && defined(__KERNEL__)
#include "mtfs_inode.h"
#include "mtfs_dentry.h"
extern int mtfs_branch_is_valid(struct inode *inode, mtfs_bindex_t bindex, __u32 valid_flags);
extern int mtfs_invalid_branch(struct inode *inode, mtfs_bindex_t bindex, __u32 valid_flags);

static inline int mtfs_i_choose_bindex(struct inode *inode, __u32 valid_flags, mtfs_bindex_t *bindex)
{
	mtfs_bindex_t i = 0;
	int ret = 0;
	HENTRY();
	
	HASSERT(inode);
	HASSERT(mtfs_valid_flags_is_valid(valid_flags));

	for (i = 0; i < mtfs_i2bnum(inode); i++) {
		ret = mtfs_branch_is_valid(inode, i, valid_flags);
		if (ret == 1) {
			*bindex = i;
			ret = 0;
			goto out;
		} else {
			continue;
		}
	}
	HERROR("None branch choosed\n");
	ret = -EINVAL;

out:
	HRETURN(ret);
}

static inline struct inode *mtfs_i_choose_branch(struct inode *inode, __u32 valid_flags)
{
	mtfs_bindex_t bindex = -1;
	struct inode *hidden_inode = NULL;
	int ret = 0;
	HENTRY();

	ret = mtfs_i_choose_bindex(inode, valid_flags, &bindex);
	if (ret) {
		hidden_inode = ERR_PTR(ret);
		goto out;
	}

	HASSERT(bindex >= 0 && bindex < mtfs_i2bnum(inode));
	hidden_inode = mtfs_i2branch(inode, bindex);
	HASSERT(hidden_inode);
out:
	HRETURN(hidden_inode);
}

static inline mtfs_bindex_t mtfs_d_choose_bindex(struct dentry *dentry, __u64 valid_flags, mtfs_bindex_t *bindex)
{
	int ret = 0;
	HENTRY();

	HASSERT(dentry);
	HASSERT(mtfs_valid_flags_is_valid(valid_flags));

	ret = mtfs_i_choose_bindex(dentry->d_inode, valid_flags, bindex);

	HRETURN(ret);
}

static inline struct dentry *mtfs_d_choose_branch(struct dentry *dentry, __u64 valid_flags)
{
	mtfs_bindex_t bindex = -1;
	struct dentry *hidden_dentry = NULL;
	int ret = 0;
	HENTRY();

	ret = mtfs_d_choose_bindex(dentry, valid_flags, &bindex);
	if (ret) {
		hidden_dentry = ERR_PTR(ret);
		goto out;
	}

	HASSERT(bindex >=0 && bindex < mtfs_d2bnum(dentry));
	hidden_dentry = mtfs_d2branch(dentry, bindex);
	HASSERT(hidden_dentry);
out:
	HRETURN(hidden_dentry);
}

static inline mtfs_bindex_t mtfs_f_choose_bindex(struct file *file, __u64 valid_flags, mtfs_bindex_t *bindex)
{
	int ret = 0;
	HENTRY();

	HASSERT(file);
	HASSERT(mtfs_valid_flags_is_valid(valid_flags));

	ret = mtfs_i_choose_bindex(file->f_dentry->d_inode, valid_flags, bindex);

	HRETURN(ret);
}

static inline struct file *mtfs_f_choose_branch(struct file *file, __u64 valid_flags)
{
	mtfs_bindex_t bindex = -1;
	struct file *hidden_file = NULL;
	int ret = 0;
	HENTRY();

	ret = mtfs_f_choose_bindex(file, valid_flags, &bindex);
	if (ret) {
		hidden_file = ERR_PTR(ret);
		goto out;
	}

	HASSERT(bindex >=0 && bindex < mtfs_f2bnum(file));
	hidden_file = mtfs_f2branch(file, bindex);
out:
	HRETURN(hidden_file);
}

extern int lowerfs_inode_get_flag_xattr(struct inode *inode, __u32 *mtfs_flag, const char *xattr_name);
extern int lowerfs_inode_set_flag_xattr(struct inode *inode, __u32 mtfs_flag, const char *xattr_name);
extern int lowerfs_inode_get_flag_default(struct inode *inode, __u32 *mtfs_flag);
extern int lowerfs_inode_set_flag_default(struct inode *inode, __u32 mtfs_flag);
#endif /* defined (__linux__) && defined(__KERNEL__) */
#endif /* __HRFS_FLAG_H__ */
