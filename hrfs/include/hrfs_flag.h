/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
 */

#ifndef __HRFS_FLAG_H__
#define __HRFS_FLAG_H__
#include <hrfs_common.h>
#include <debug.h>
#include <hrfs_file.h>
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

#if defined (__linux__) && defined(__KERNEL__)
#include <hrfs_inode.h>
#include <hrfs_dentry.h>
extern int hrfs_branch_is_valid(struct inode *inode, hrfs_bindex_t bindex, __u32 valid_flags);
extern int hrfs_i_invalid_branch(struct inode *inode, hrfs_bindex_t bindex, __u32 valid_flags);

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

static inline hrfs_bindex_t hrfs_f_choose_bindex(struct file *file, __u64 valid_flags, hrfs_bindex_t *bindex)
{
	int ret = 0;

	HASSERT(file);
	HASSERT(hrfs_valid_flags_is_valid(valid_flags));

	ret = hrfs_i_choose_bindex(file->f_dentry->d_inode, valid_flags, bindex);

	return ret;
}

static inline struct file *hrfs_f_choose_branch(struct file *file, __u64 valid_flags)
{
	hrfs_bindex_t bindex = -1;
	struct file *hidden_file = NULL;
	int ret = 0;

	ret = hrfs_f_choose_bindex(file, valid_flags, &bindex);
	if (ret <= 0) {
		hidden_file = ERR_PTR(ret);
		goto out;
	}

	HASSERT(bindex >=0 && bindex < hrfs_f2bnum(file));
	hidden_file = hrfs_f2branch(file, bindex);
out:
	return hidden_file;
}

typedef union hrfs_operation_result {
	int ret;
	void *ptr;
	ssize_t size;
} hrfs_operation_result_t;

struct hrfs_operation_binfo {
	hrfs_bindex_t bindex;     /* Global bindex */
	int is_suceessful;        /* Whether this branch succeeded */
	hrfs_operation_result_t result;
	int valid;
};

struct hrfs_operation_list {
	hrfs_bindex_t bnum;                     /* Branch number */
	hrfs_bindex_t checked_bnum;             /* Number of nonlatest branches failed */
	hrfs_bindex_t latest_bnum;              /* Number of latest branches */
	struct hrfs_operation_binfo *op_binfo;  /* Global bindex */
	hrfs_bindex_t valid_bnum;               /* Number of valid branches */
	hrfs_bindex_t success_bnum;             /* Number of branches succeeded */
	hrfs_bindex_t success_latest_bnum;      /* Number of latest branches succeeded */
	hrfs_bindex_t success_nonlatest_bnum;   /* Number of nonlatest branches succeeded */
	hrfs_bindex_t fault_bnum;               /* Number of branches failed */
	hrfs_bindex_t fault_latest_bnum;        /* Number of latest branches failed */
	hrfs_bindex_t fault_nonlatest_bnum;     /* Number of nonlatest branches failed */
};

#include <memory.h>
static inline struct hrfs_operation_list *hrfs_oplist_alloc(hrfs_bindex_t bnum)
{
	struct hrfs_operation_list *list = NULL;

	HRFS_ALLOC_PTR(list);
	if (unlikely(list == NULL)) {
		goto out;
	}
	
	list->bnum = bnum;
	HRFS_ALLOC(list->op_binfo, sizeof(*list->op_binfo) * bnum);
	if (unlikely(list->op_binfo == NULL)) {
		goto out_free_list;
	}
	goto out;
out_free_list:
	HRFS_FREE_PTR(list);
out:
	return list;
}

static inline void hrfs_oplist_free(struct hrfs_operation_list *list)
{
	HRFS_FREE(list->op_binfo, sizeof(*list->op_binfo) * list->bnum);
	HRFS_FREE_PTR(list);
}

static inline void hrfs_oplist_dump(struct hrfs_operation_list *list)
{
	hrfs_bindex_t bindex = 0;

	HPRINT("oplist bnum = %d\n", list->bnum);
	HPRINT("oplist latest_bnum = %d\n", list->latest_bnum);
	for (bindex = 0; bindex < list->bnum; bindex++) {
		HPRINT("branch[%d]: %d\n", bindex, list->op_binfo[bindex].bindex);
	}
}

static inline struct hrfs_operation_list *hrfs_oplist_build(struct inode *inode)
{
	struct hrfs_operation_list *list = NULL;
	hrfs_bindex_t bindex = 0;
	hrfs_bindex_t bnum = hrfs_i2bnum(inode);
	hrfs_bindex_t blast = bnum - 1;
	hrfs_bindex_t bfirst = 0;
	int is_valid = 1;
	struct hrfs_operation_binfo *binfo = NULL;

	list = hrfs_oplist_alloc(bnum);
	if (unlikely(list == NULL)) {
		goto out;
	}

	/* NOT a good implementation, change me */
	for (bindex = 0; bindex < bnum; bindex++) {
		is_valid = hrfs_branch_is_valid(inode, bindex, HRFS_DATA_VALID);
		if (is_valid) {
			binfo = &(list->op_binfo[bfirst]);
			bfirst++;
		} else {
			binfo = &(list->op_binfo[blast]);
			blast--;
		}
		binfo->bindex = bindex;
	}
	list->latest_bnum = bfirst;
	hrfs_oplist_dump(list);
out:
	return list;
}

static inline int hrfs_oplist_check(struct hrfs_operation_list *list)
{
	hrfs_bindex_t bindex = 0;
	int ret = 0;
	HENTRY();

	if (list->checked_bnum == 0) {
		HASSERT(list->valid_bnum == 0);
		HASSERT(list->success_bnum == 0);
		HASSERT(list->success_latest_bnum == 0);
		HASSERT(list->success_nonlatest_bnum == 0);
		HASSERT(list->fault_bnum == 0);
		HASSERT(list->fault_latest_bnum == 0);
		HASSERT(list->fault_nonlatest_bnum == 0);
	}

	for (bindex = list->checked_bnum; bindex < list->bnum; bindex++) {
		if (!list->op_binfo[bindex].valid) {
			break;
		}

		list->checked_bnum++;
		list->valid_bnum++;
		if (list->op_binfo[bindex].is_suceessful) {
			list->success_bnum++;
			if (bindex < list->latest_bnum) {
				list->success_latest_bnum++;
			} else {
				list->success_nonlatest_bnum++;
			}
		} else {
			list->fault_bnum++;
			if (bindex < list->latest_bnum) {
				list->fault_latest_bnum++;
			} else {
				list->fault_nonlatest_bnum++;
			}
		}
	}
	HASSERT(list->fault_latest_bnum + list->fault_nonlatest_bnum == list->fault_bnum);
	HASSERT(list->success_latest_bnum + list->success_nonlatest_bnum == list->success_bnum);
	HASSERT(list->success_bnum + list->fault_bnum == list->valid_bnum);
	HASSERT(list->valid_bnum <= list->bnum);
	HASSERT(list->checked_bnum <= list->bnum);
	HRETURN(ret);
}

static inline int hrfs_oplist_setbranch(struct hrfs_operation_list *list, hrfs_bindex_t bindex, int is_successful, hrfs_operation_result_t result)
{
	HASSERT(bindex >= 0 && bindex < list->bnum);
	list->op_binfo[bindex].valid = 1;
	list->op_binfo[bindex].is_suceessful = is_successful;
	list->op_binfo[bindex].result = result;
	return 0;
}

/* Choose a operation status returned */
static inline hrfs_operation_result_t hrfs_oplist_result(struct hrfs_operation_list *list)
{
	return list->op_binfo[0].result;
}

static inline int hrfs_oplist_update(struct inode *inode, struct hrfs_operation_list *list)
{
	hrfs_bindex_t bindex = 0;
	hrfs_bindex_t i = 0;
	int ret = 0;
	HENTRY();

	HASSERT(list->checked_bnum > 0);
	HASSERT(list->valid_bnum > 0);
	HASSERT(list->success_latest_bnum > 0);
	if (list->fault_latest_bnum == 0) {
		goto out;
	}

	for (i = 0; i < list->bnum; i++) {
		bindex = list->op_binfo[i].bindex;
		if (!list->op_binfo[i].valid) {
			break;
		}

		if (!list->op_binfo[i].is_suceessful) {
			if (hrfs_i2branch(inode, bindex) == NULL) {
				continue;
			}

			ret = hrfs_i_invalid_branch(inode, bindex, HRFS_DATA_VALID);
			if (ret) {
				HERROR("invalid inode failed, ret = %d\n", ret);
				HBUG();
			}
		}
	}
out:
	HRETURN(ret);
}
#endif /* defined (__linux__) && defined(__KERNEL__) */
#endif /* __HRFS_FLAG_H__ */
