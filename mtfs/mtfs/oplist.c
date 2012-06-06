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

struct mtfs_operation_list *mtfs_oplist_build(struct inode *inode)
{
	struct mtfs_operation_list *list = NULL;
	mtfs_bindex_t bindex = 0;
	mtfs_bindex_t bnum = mtfs_i2bnum(inode);
	mtfs_bindex_t blast = bnum - 1;
	mtfs_bindex_t bfirst = 0;
	int is_valid = 1;
	struct mtfs_operation_binfo *binfo = NULL;

	list = mtfs_oplist_alloc(bnum);
	if (unlikely(list == NULL)) {
		goto out;
	}

	/* NOT a good implementation, change me */
	for (bindex = 0; bindex < bnum; bindex++) {
		is_valid = mtfs_branch_is_valid(inode, bindex, MTFS_DATA_VALID);
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
out:
	return list;
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
