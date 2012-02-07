/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
 */
#include <memory.h>
#include <hrfs_oplist.h>
#include <hrfs_inode.h>
#include <hrfs_flag.h>
#include "main_internal.h"

static struct hrfs_operation_list *hrfs_oplist_alloc(hrfs_bindex_t bnum)
{
	struct hrfs_operation_list *list = NULL;

	HRFS_SLAB_ALLOC_PTR(list, hrfs_oplist_cache);	
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
	HRFS_SLAB_FREE_PTR(list, hrfs_oplist_cache);
out:
	return list;
}

void hrfs_oplist_free(struct hrfs_operation_list *list)
{
	HRFS_FREE(list->op_binfo, sizeof(*list->op_binfo) * list->bnum);
	HRFS_SLAB_FREE_PTR(list, hrfs_oplist_cache);
}
EXPORT_SYMBOL(hrfs_oplist_free);

static inline void hrfs_oplist_dump(struct hrfs_operation_list *list)
{
	hrfs_bindex_t bindex = 0;

	HPRINT("oplist bnum = %d\n", list->bnum);
	HPRINT("oplist latest_bnum = %d\n", list->latest_bnum);
	for (bindex = 0; bindex < list->bnum; bindex++) {
		HPRINT("branch[%d]: %d\n", bindex, list->op_binfo[bindex].bindex);
	}
}

struct hrfs_operation_list *hrfs_oplist_build(struct inode *inode)
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
EXPORT_SYMBOL(hrfs_oplist_build);

int hrfs_oplist_check(struct hrfs_operation_list *list)
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
EXPORT_SYMBOL(hrfs_oplist_check);

int hrfs_oplist_setbranch(struct hrfs_operation_list *list,
                          hrfs_bindex_t bindex,
                          int is_successful,
                          hrfs_operation_result_t result)
{
	HASSERT(bindex >= 0 && bindex < list->bnum);
	list->op_binfo[bindex].valid = 1;
	list->op_binfo[bindex].is_suceessful = is_successful;
	list->op_binfo[bindex].result = result;
	return 0;
}
EXPORT_SYMBOL(hrfs_oplist_setbranch);

/* Choose a operation status returned */
hrfs_operation_result_t hrfs_oplist_result(struct hrfs_operation_list *list)
{
	return list->op_binfo[0].result;
}
EXPORT_SYMBOL(hrfs_oplist_result);

int hrfs_oplist_update(struct inode *inode, struct hrfs_operation_list *list)
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

			ret = hrfs_invalid_branch(inode, bindex, HRFS_DATA_VALID);
			if (ret) {
				HERROR("invalid inode failed, ret = %d\n", ret);
				HBUG();
			}
		}
	}
out:
	HRETURN(ret);
}
EXPORT_SYMBOL(hrfs_oplist_update);
