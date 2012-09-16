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

	HASSERT(bnum > 0 && bnum <= MTFS_BRANCH_MAX);
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

	HPRINT("oplist bnum = %d\n", list->bnum);
	HPRINT("oplist latest_bnum = %d\n", list->latest_bnum);
	for (bindex = 0; bindex < list->bnum; bindex++) {
		HPRINT("branch[%d]: %d\n", bindex, list->op_binfo[bindex].bindex);
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
	HENTRY();

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

	HRETURN(ret);
}
EXPORT_SYMBOL(mtfs_oplist_init);

struct mtfs_operation_list *mtfs_oplist_build(struct inode *inode)
{
	struct mtfs_operation_list *oplist = NULL;
	mtfs_bindex_t bnum = mtfs_i2bnum(inode);
	int ret = 0;
	HENTRY();
	
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
	HRETURN(oplist);
}
EXPORT_SYMBOL(mtfs_oplist_build);

int mtfs_oplist_check(struct mtfs_operation_list *list)
{
	mtfs_bindex_t bindex = 0;
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
	HENTRY();

	if (oplist->checked_bnum == 0) {
		HASSERT(oplist->valid_bnum == 0);
		HASSERT(oplist->success_bnum == 0);
		HASSERT(oplist->success_latest_bnum == 0);
		HASSERT(oplist->success_nonlatest_bnum == 0);
		HASSERT(oplist->fault_bnum == 0);
		HASSERT(oplist->fault_latest_bnum == 0);
		HASSERT(oplist->fault_nonlatest_bnum == 0);
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
	HASSERT(oplist->fault_latest_bnum + oplist->fault_nonlatest_bnum == oplist->fault_bnum);
	HASSERT(oplist->success_latest_bnum + oplist->success_nonlatest_bnum == oplist->success_bnum);
	HASSERT(oplist->success_bnum + oplist->fault_bnum == oplist->valid_bnum);
	HASSERT(oplist->valid_bnum <= oplist->bnum);
	HASSERT(oplist->checked_bnum <= oplist->bnum);

	if (oplist->opinfo == NULL) {
		oplist->opinfo = &(oplist->op_binfo[0]);
	} else if (bindex_chosed != -1) {
		oplist->opinfo = &(oplist->op_binfo[bindex_chosed]);
	}
	_HRETURN();


}
EXPORT_SYMBOL(mtfs_oplist_merge);

int mtfs_oplist_update(struct inode *inode, struct mtfs_operation_list *list)
{
	mtfs_bindex_t bindex = 0;
	mtfs_bindex_t i = 0;
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
			if (mtfs_i2branch(inode, bindex) == NULL) {
				continue;
			}

			ret = mtfs_invalid_branch(inode, bindex, MTFS_DATA_VALID);
			if (ret) {
				HERROR("invalid inode failed, ret = %d\n", ret);
				HBUG();
			}
		}
	}
out:
	HRETURN(ret);
}
EXPORT_SYMBOL(mtfs_oplist_update);
