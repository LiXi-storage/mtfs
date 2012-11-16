/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <memory.h>
#include <mtfs_oplist.h>
#include <mtfs_inode.h>
#include <mtfs_flag.h>
#include <linux/module.h>
#include "main_internal.h"

int mtfs_oplist_init_flag(struct mtfs_operation_list *oplist, struct inode *inode)
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
	MRETURN(ret);
}

int mtfs_oplist_flush_flag(struct mtfs_operation_list *oplist,
                           struct inode *inode)
{
	mtfs_bindex_t bindex = 0;
	mtfs_bindex_t i = 0;
	int ret = 0;
	MENTRY();
	
	/*
	 * Degradate only if some of latest branches failed
	 * and others succeeded 
	 */
	if (oplist->success_latest_bnum == 0 ||
	    oplist->fault_latest_bnum == 0) {
		goto out;
	}

	/*
	 * During write_ops, some non-latest branches
	 * may be skipped and not valid. 
	 */
	for (i = 0; i < oplist->valid_bnum; i++) {
		bindex = oplist->op_binfo[i].bindex;
		MASSERT(oplist->op_binfo[i].valid);

		if (!oplist->op_binfo[i].is_suceessful) {
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

int mtfs_oplist_gather_default(struct mtfs_operation_list *oplist)
{
	mtfs_bindex_t bindex = 0;
	mtfs_bindex_t bindex_chosed = -1;
	int ret = 0;
	MENTRY();

	MASSERT(oplist->fault_latest_bnum + oplist->fault_nonlatest_bnum == oplist->fault_bnum);
	MASSERT(oplist->success_latest_bnum + oplist->success_nonlatest_bnum == oplist->success_bnum);
	MASSERT(oplist->success_bnum + oplist->fault_bnum == oplist->valid_bnum);
	MASSERT(oplist->valid_bnum <= oplist->bnum);

	for (bindex = 0; bindex < oplist->valid_bnum; bindex++) {
		MASSERT(oplist->op_binfo[bindex].valid);
		if (oplist->op_binfo[bindex].is_suceessful) {
			bindex_chosed = bindex;
			break;
		}
	}

	if (bindex_chosed != -1) {
		oplist->opinfo = &(oplist->op_binfo[bindex_chosed]);
	} else if (oplist->fault_bnum) {
		oplist->opinfo = &(oplist->op_binfo[0]);
	}

	MRETURN(ret);
}

struct mtfs_operation_list *mtfs_oplist_alloc(mtfs_bindex_t bnum)
{
	struct mtfs_operation_list *oplist = NULL;

	MASSERT(bnum > 0 && bnum <= MTFS_BRANCH_MAX);
	MTFS_SLAB_ALLOC_PTR(oplist, mtfs_oplist_cache);	
	if (unlikely(oplist == NULL)) {
		goto out;
	}
	oplist->bnum = bnum;

out:
	return oplist;
}
EXPORT_SYMBOL(mtfs_oplist_alloc);

void mtfs_oplist_free(struct mtfs_operation_list *oplist)
{
	MTFS_SLAB_FREE_PTR(oplist, mtfs_oplist_cache);
}
EXPORT_SYMBOL(mtfs_oplist_free);

static inline void mtfs_oplist_dump(struct mtfs_operation_list *oplist)
{
	mtfs_bindex_t bindex = 0;

	MPRINT("oplist bnum = %d\n", oplist->bnum);
	MPRINT("oplist latest_bnum = %d\n", oplist->latest_bnum);
	for (bindex = 0; bindex < oplist->bnum; bindex++) {
		MPRINT("branch[%d]: %d\n", bindex, oplist->op_binfo[bindex].bindex);
	}
}

struct mtfs_operation_list *mtfs_oplist_build(struct inode *inode,
                                              struct mtfs_oplist_object *oplist_obj)
{
	struct mtfs_operation_list *oplist = NULL;
	mtfs_bindex_t bnum = mtfs_i2bnum(inode);
	int ret = 0;
	MENTRY();
	
	oplist = mtfs_oplist_alloc(bnum);
	if (unlikely(oplist == NULL)) {
		goto out;
	}

	ret = mtfs_oplist_init(oplist, inode, oplist_obj);
	if (unlikely(ret)) {
		mtfs_oplist_free(oplist);
		oplist = NULL;
	}	
out:
	MRETURN(oplist);
}
EXPORT_SYMBOL(mtfs_oplist_build);

int mtfs_oplist_init_sequential(struct mtfs_operation_list *list, struct inode *inode)
{
	mtfs_bindex_t bindex = 0;
	mtfs_bindex_t bnum = mtfs_i2bnum(inode);
	int ret = 0;
	MENTRY();

	for (bindex = 0; bindex < bnum; bindex++) {
	        list->op_binfo[bindex].bindex = bindex;
	}
	list->latest_bnum = bnum;

	MRETURN(ret);
}

int mtfs_oplist_init(struct mtfs_operation_list *oplist,
                     struct inode *inode,
                     struct mtfs_oplist_object *oplist_obj)
{
	int ret = 0;
	MENTRY();

	MASSERT(oplist_obj->mopo_init);
	ret = oplist_obj->mopo_init(oplist, inode);
	if (ret) {
		MERROR("failed to init oplist\n");
		goto out;
	}
	oplist->bnum = mtfs_i2bnum(inode);
	oplist->mopl_type = oplist_obj;
	oplist->inited = 1;
out:
	MRETURN(ret);
}
EXPORT_SYMBOL(mtfs_oplist_init);

int mtfs_oplist_flush(struct mtfs_operation_list *oplist,
                      struct inode *inode)
{
	int ret = 0;
	MENTRY();

	if (oplist->mopl_type->mopo_flush) {
		ret = oplist->mopl_type->mopo_flush(oplist, inode);
		if (ret) {
			MERROR("failed to flush oplist\n");
		}
	}

	MRETURN(ret);
}
EXPORT_SYMBOL(mtfs_oplist_flush);

int mtfs_oplist_setbranch(struct mtfs_operation_list *oplist,
                          mtfs_bindex_t bindex,
                          int is_successful,
                          mtfs_operation_result_t result)
{
	MASSERT(bindex >= 0 && bindex < oplist->bnum);
	oplist->op_binfo[bindex].valid = 1;
	oplist->op_binfo[bindex].is_suceessful = is_successful;
	oplist->op_binfo[bindex].result = result;

	oplist->valid_bnum++;

	if (oplist->op_binfo[bindex].is_suceessful) {
		oplist->success_bnum++;
		if (bindex < oplist->latest_bnum) {
			oplist->success_latest_bnum++;
		} else {
			oplist->success_nonlatest_bnum++;
		}
	} else {
		oplist->fault_bnum++;
		if (bindex < oplist->latest_bnum) {
			oplist->fault_latest_bnum++;
		} else {
			oplist->fault_nonlatest_bnum++;
		}
	}
	return 0;
}
EXPORT_SYMBOL(mtfs_oplist_setbranch);

/* Choose a operation status returned */
int mtfs_oplist_gather(struct mtfs_operation_list *oplist)
{
	int ret = 0;
	MENTRY();

	MASSERT(oplist->fault_latest_bnum + oplist->fault_nonlatest_bnum == oplist->fault_bnum);
	MASSERT(oplist->success_latest_bnum + oplist->success_nonlatest_bnum == oplist->success_bnum);
	MASSERT(oplist->success_bnum + oplist->fault_bnum == oplist->valid_bnum);
	MASSERT(oplist->valid_bnum <= oplist->bnum);

	MASSERT(oplist->mopl_type->mopo_gather);
	ret = oplist->mopl_type->mopo_gather(oplist);
	if (ret) {
		MERROR("failed to gather oplist\n");
	}

	MRETURN(ret);
}
EXPORT_SYMBOL(mtfs_oplist_gather);

/* Choose a operation status returned */
mtfs_operation_result_t mtfs_oplist_result(struct mtfs_operation_list *oplist)
{
	MASSERT(oplist->opinfo);
	return oplist->opinfo->result;
}
EXPORT_SYMBOL(mtfs_oplist_result);

struct mtfs_oplist_object mtfs_oplist_flag = {
	.mopo_init     = mtfs_oplist_init_flag,
	.mopo_flush    = mtfs_oplist_flush_flag,
	.mopo_gather   = mtfs_oplist_gather_default,
};
EXPORT_SYMBOL(mtfs_oplist_flag);

struct mtfs_oplist_object mtfs_oplist_sequential = {
	.mopo_init     = mtfs_oplist_init_sequential,
	.mopo_flush    = mtfs_oplist_flush_flag,
	.mopo_gather   = mtfs_oplist_gather_default,
};
EXPORT_SYMBOL(mtfs_oplist_sequential);

int mtfs_oplist_gather_master(struct mtfs_operation_list *oplist)
{
	mtfs_bindex_t bindex = 0;
	mtfs_bindex_t bindex_chosed = -1;
	int ret = 0;
	MENTRY();

	MASSERT(oplist->fault_latest_bnum + oplist->fault_nonlatest_bnum == oplist->fault_bnum);
	MASSERT(oplist->success_latest_bnum + oplist->success_nonlatest_bnum == oplist->success_bnum);
	MASSERT(oplist->success_bnum + oplist->fault_bnum == oplist->valid_bnum);
	MASSERT(oplist->valid_bnum <= oplist->bnum);

	oplist->opinfo = &(oplist->op_binfo[0]);

	MRETURN(ret);
}

struct mtfs_oplist_object mtfs_oplist_master = {
	.mopo_init     = mtfs_oplist_init_sequential,
	.mopo_flush    = NULL,
	.mopo_gather   = mtfs_oplist_gather_master,
};
EXPORT_SYMBOL(mtfs_oplist_master);

