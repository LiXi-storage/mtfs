/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <memory.h>
#include <mtfs_oplist.h>
#include <mtfs_inode.h>
#include <mtfs_flag.h>
#include <linux/module.h>
#include "main_internal.h"

struct mtfs_operation_list *mtfs_oplist_alloc(mtfs_bindex_t bnum)
{
	struct mtfs_operation_list *list = NULL;

	MASSERT(bnum > 0 && bnum <= MTFS_BRANCH_MAX);
	MTFS_SLAB_ALLOC_PTR(list, mtfs_oplist_cache);	
	if (unlikely(list == NULL)) {
		goto out;
	}
	list->bnum = bnum;

out:
	return list;
}
EXPORT_SYMBOL(mtfs_oplist_alloc);

void mtfs_oplist_free(struct mtfs_operation_list *list)
{
	MTFS_SLAB_FREE_PTR(list, mtfs_oplist_cache);
}
EXPORT_SYMBOL(mtfs_oplist_free);

static inline void mtfs_oplist_dump(struct mtfs_operation_list *list)
{
	mtfs_bindex_t bindex = 0;

	MPRINT("oplist bnum = %d\n", list->bnum);
	MPRINT("oplist latest_bnum = %d\n", list->latest_bnum);
	for (bindex = 0; bindex < list->bnum; bindex++) {
		MPRINT("branch[%d]: %d\n", bindex, list->op_binfo[bindex].bindex);
	}
}

struct mtfs_operation_list *mtfs_oplist_build_keep_order(struct inode *inode)
{
	struct mtfs_operation_list *list = NULL;
	mtfs_bindex_t bindex = 0;
	mtfs_bindex_t bnum = mtfs_i2bnum(inode);

	list = mtfs_oplist_alloc(bnum);
	if (unlikely(list == NULL)) {
		goto out;
	}

	for (bindex = 0; bindex < bnum; bindex++) {
	        list->op_binfo[bindex].bindex = bindex;
	}
	list->latest_bnum = bnum;
out:
	return list;
}
EXPORT_SYMBOL(mtfs_oplist_build_keep_order);

int mtfs_oplist_init(struct mtfs_operation_list *oplist, struct inode *inode)
{
	mtfs_bindex_t bindex = 0;
	mtfs_bindex_t bnum = mtfs_i2bnum(inode);
	mtfs_bindex_t blast = bnum - 1;
	mtfs_bindex_t bfirst = 0;
	int is_valid = 1;
	struct mtfs_operation_binfo *binfo = NULL;
	int ret = 0;
	MENTRY();

	/* NOT a good implementation, change me */
	for (bindex = 0; bindex < bnum; bindex++) {
		is_valid = mtfs_branch_is_valid(inode, bindex, MTFS_DATA_VALID);
		if (is_valid) {
			binfo = &(oplist->op_binfo[bfirst]);
			bfirst++;
		} else {
			binfo = &(oplist->op_binfo[blast]);
			blast--;
		}
		binfo->bindex = bindex;
	}
	oplist->latest_bnum = bfirst;
	oplist->bnum = bnum;

	MRETURN(ret);
}
EXPORT_SYMBOL(mtfs_oplist_init);

struct mtfs_operation_list *mtfs_oplist_build(struct inode *inode)
{
	struct mtfs_operation_list *oplist = NULL;
	mtfs_bindex_t bnum = mtfs_i2bnum(inode);
	int ret = 0;
	MENTRY();
	
	oplist = mtfs_oplist_alloc(bnum);
	if (unlikely(oplist == NULL)) {
		goto out;
	}

	ret = mtfs_oplist_init(oplist, inode);
	if (unlikely(ret)) {
		mtfs_oplist_free(oplist);
		oplist = NULL;
	}	
out:
	MRETURN(oplist);
}
EXPORT_SYMBOL(mtfs_oplist_build);

int mtfs_oplist_check(struct mtfs_operation_list *list)
{
	mtfs_bindex_t bindex = 0;
	int ret = 0;
	MENTRY();

	if (list->checked_bnum == 0) {
		MASSERT(list->valid_bnum == 0);
		MASSERT(list->success_bnum == 0);
		MASSERT(list->success_latest_bnum == 0);
		MASSERT(list->success_nonlatest_bnum == 0);
		MASSERT(list->fault_bnum == 0);
		MASSERT(list->fault_latest_bnum == 0);
		MASSERT(list->fault_nonlatest_bnum == 0);
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
	MASSERT(list->fault_latest_bnum + list->fault_nonlatest_bnum == list->fault_bnum);
	MASSERT(list->success_latest_bnum + list->success_nonlatest_bnum == list->success_bnum);
	MASSERT(list->success_bnum + list->fault_bnum == list->valid_bnum);
	MASSERT(list->valid_bnum <= list->bnum);
	MASSERT(list->checked_bnum <= list->bnum);
	MRETURN(ret);
}
EXPORT_SYMBOL(mtfs_oplist_check);

/* Choose a operation status returned */
mtfs_operation_result_t mtfs_oplist_result(struct mtfs_operation_list *list)
{
	mtfs_bindex_t bindex = 0;
	if (list->success_bnum > 0) {
		for (bindex = 0; bindex < list->bnum; bindex++) {
			if (list->op_binfo[bindex].is_suceessful) {
				break;
			}
		}
	}
	return list->op_binfo[bindex].result;
}
EXPORT_SYMBOL(mtfs_oplist_result);

/* Choose a operation status returned */
void mtfs_oplist_merge(struct mtfs_operation_list *oplist)
{
	mtfs_bindex_t bindex = 0;
	mtfs_bindex_t bindex_chosed = -1;
	MENTRY();

	if (oplist->checked_bnum == 0) {
		MASSERT(oplist->valid_bnum == 0);
		MASSERT(oplist->success_bnum == 0);
		MASSERT(oplist->success_latest_bnum == 0);
		MASSERT(oplist->success_nonlatest_bnum == 0);
		MASSERT(oplist->fault_bnum == 0);
		MASSERT(oplist->fault_latest_bnum == 0);
		MASSERT(oplist->fault_nonlatest_bnum == 0);
	}

	for (bindex = oplist->checked_bnum; bindex < oplist->bnum; bindex++) {
		if (!oplist->op_binfo[bindex].valid) {
			break;
		}

		oplist->checked_bnum++;
		oplist->valid_bnum++;
		if (oplist->op_binfo[bindex].is_suceessful) {
			oplist->success_bnum++;
			if (bindex < oplist->latest_bnum) {
				oplist->success_latest_bnum++;
			} else {
				oplist->success_nonlatest_bnum++;
			}
			bindex_chosed = bindex;
		} else {
			oplist->fault_bnum++;
			if (bindex < oplist->latest_bnum) {
				oplist->fault_latest_bnum++;
			} else {
				oplist->fault_nonlatest_bnum++;
			}
		}
	}
	MASSERT(oplist->fault_latest_bnum + oplist->fault_nonlatest_bnum == oplist->fault_bnum);
	MASSERT(oplist->success_latest_bnum + oplist->success_nonlatest_bnum == oplist->success_bnum);
	MASSERT(oplist->success_bnum + oplist->fault_bnum == oplist->valid_bnum);
	MASSERT(oplist->valid_bnum <= oplist->bnum);
	MASSERT(oplist->checked_bnum <= oplist->bnum);

	if (oplist->opinfo == NULL) {
		oplist->opinfo = &(oplist->op_binfo[0]);
	} else if (bindex_chosed != -1) {
		oplist->opinfo = &(oplist->op_binfo[bindex_chosed]);
	}

	_MRETURN();


}
EXPORT_SYMBOL(mtfs_oplist_merge);

int mtfs_oplist_update(struct inode *inode, struct mtfs_operation_list *list)
{
	mtfs_bindex_t bindex = 0;
	mtfs_bindex_t i = 0;
	int ret = 0;
	MENTRY();

	/* All valid branches should have been checked */
	MASSERT(list->valid_bnum == list->checked_bnum);
	
	/*
	 * Degradate only if some of latest branches failed
	 * and others succeeded 
	 */
	if (list->success_latest_bnum == 0 ||
	    list->fault_latest_bnum == 0) {
		goto out;
	}

	/*
	 * During write_ops, some non-latest branches
	 * may be skipped and not checked. 
	 */
	for (i = 0; i < list->checked_bnum; i++) {
		bindex = list->op_binfo[i].bindex;
		if (!list->op_binfo[i].valid) {
			/*
			 * No one after me is valid.
			 */
			break;
		}

		if (!list->op_binfo[i].is_suceessful) {
			if (mtfs_i2branch(inode, bindex) == NULL) {
				continue;
			}

			ret = mtfs_invalid_branch(inode, bindex, MTFS_DATA_VALID);
			if (ret) {
				MERROR("invalid inode failed, ret = %d\n", ret);
				MBUG();
			}
		}
	}
out:
	MRETURN(ret);
}
EXPORT_SYMBOL(mtfs_oplist_update);
