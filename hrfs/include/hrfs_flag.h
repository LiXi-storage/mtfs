/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
 */

#ifndef __HRFS_FLAG_H__
#define __HRFS_FLAG_H__
#include <hrfs_common.h>
#include <debug.h>
/*
 * Same to macros defined in swgfs/include/swgfs_idl.h
 * We should keep this ture:
 * When flag is zero by default,
 * it means file data is proper and not under recovering.
 * Lowest 4 bits are for raid_pattern.
 */
#define HRFS_FLAG_PRIMARY                0x00000010
#define HRFS_FLAG_DATABAD                0x00000020
#define HRFS_FLAG_RECOVERING             0x00000040
#define HRFS_FLAG_SETED                  0x00000080

#define HRFS_FLAG_MDS_MASK               0x000000ff
#define HRFS_FLAG_MDS_SYMBOL             0xc0ffee00

#define HRFS_FLAG_RAID_MASK              0x0000000f

static inline int hrfs_flag_is_valid(__u32 hrfs_flag)
{
	int rc = 0;

	if (((hrfs_flag & HRFS_FLAG_SETED) == 0) &&
	    (hrfs_flag != 0)) {
		/* If not seted, then it is zero */
		goto out;
	}

	if ((hrfs_flag & (~HRFS_FLAG_MDS_MASK)) != 0) {
		/* Should not set unused flag*/
		goto out;
	}

	if ((hrfs_flag & HRFS_FLAG_RECOVERING) &&
	    (!(hrfs_flag & HRFS_FLAG_DATABAD))) {
		/* Branch being recovered should be bad*/
		goto out;
	}
	rc = 1;
out:
	return rc;
}

#define HRFS_DATA_BIT     0x00001
#define HRFS_ATTR_BIT     0x00002
#define HRFS_XATTR_BIT    0x00004
#define HRFS_BRANCH_BIT    0x00008

#define HRFS_DATA_VALID   (HRFS_DATA_BIT)
#define HRFS_ATTR_VALID   (HRFS_ATTR_BIT)
#define HRFS_XATTR_VALID  (HRFS_XATTR_BIT)
#define HRFS_BRANCH_VALID  (HRFS_BRANCH_BIT)
#define HRFS_ALL_VALID    (HRFS_DATA_BIT | HRFS_ATTR_BIT | HRFS_XATTR_BIT | HRFS_BRANCH_VALID)

static inline int hrfs_valid_flags_is_valid(__u32 valid_flags)
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

struct hrfs_operation_binfo {
	hrfs_bindex_t bindex;     /* Global bindex */
	int is_suceessful;        /* Whether this branch is suceessful */
};

struct hrfs_operation_list {
	hrfs_bindex_t bnum;                     /* Branch number */
	hrfs_bindex_t latest_bnum;              /* Number of latest branches */
	struct hrfs_operation_binfo *op_binfo; /* Global bindex */
	int is_suceessful;                      /* Some branches succeeded */
	int have_fault;                         /* Some branches failed */
};

#if defined (__linux__) && defined(__KERNEL__)
#include <hrfs_inode.h>
#include <hrfs_dentry.h>
extern int hrfs_branch_is_valid(struct inode *inode, hrfs_bindex_t bindex, __u32 valid_flags);

/*
 * 1: valid
 * 0: not valid
 * -errno: error
 */
static inline int hrfs_i_choose_bindex(struct inode *inode, __u32 valid_flags, hrfs_bindex_t *bindex)
{
	hrfs_bindex_t i = 0;
	int ret = 0;
	
	HASSERT(inode);
	HASSERT(hrfs_valid_flags_is_valid(valid_flags));

	for (i = 0; i < hrfs_i2bnum(inode); i++) {
		ret = hrfs_branch_is_valid(inode, i, valid_flags);
		if (ret == 1) {
			*bindex = i;
			break;
		} else {
			continue;
		}
	}

	return ret;	
}

static inline struct inode *hrfs_i_choose_branch(struct inode *inode, __u32 valid_flags)
{
	hrfs_bindex_t bindex = -1;
	struct inode *hidden_inode = NULL;
	int ret = 0;

	ret = hrfs_i_choose_bindex(inode, valid_flags, &bindex);
	if (ret <= 0) {
		hidden_inode = ERR_PTR(ret);
		goto out;
	}

	HASSERT(bindex >= 0 && bindex < hrfs_i2bnum(inode));
	hidden_inode = hrfs_i2branch(inode, bindex);
out:
	return hidden_inode;
}

static inline hrfs_bindex_t hrfs_d_choose_bindex(struct dentry *dentry, __u64 valid_flags, hrfs_bindex_t *bindex)
{
	int ret = 0;

	HASSERT(dentry);
	HASSERT(hrfs_valid_flags_is_valid(valid_flags));

	ret = hrfs_i_choose_bindex(dentry->d_inode, valid_flags, bindex);

	return ret;
}

static inline struct dentry *hrfs_d_choose_branch(struct dentry *dentry, __u64 valid_flags)
{
	hrfs_bindex_t bindex = -1;
	struct dentry *hidden_dentry = NULL;
	int ret = 0;

	ret = hrfs_d_choose_bindex(dentry, valid_flags, &bindex);
	if (ret <= 0) {
		hidden_dentry = ERR_PTR(ret);
		goto out;
	}

	HASSERT(bindex >=0 && bindex < hrfs_d2bnum(dentry));
	hidden_dentry = hrfs_d2branch(dentry, bindex);
out:
	return hidden_dentry;
}
#endif /* defined (__linux__) && defined(__KERNEL__) */
#endif /* __HRFS_FLAG_H__ */
