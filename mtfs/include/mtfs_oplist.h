/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __HRFS_OPLIST_H__
#define __HRFS_OPLIST_H__
#include "mtfs_common.h"
#include "debug.h"
#if defined (__linux__) && defined(__KERNEL__)
typedef union mtfs_operation_result {
	int ret;
	void *ptr;
	ssize_t size;
} mtfs_operation_result_t;

struct mtfs_operation_binfo {
	mtfs_bindex_t bindex;     /* Global bindex */
	int is_suceessful;        /* Whether this branch succeeded */
	mtfs_operation_result_t result;
	int valid;
};

struct mtfs_operation_list {
	mtfs_bindex_t bnum;                     /* Branch number */
	mtfs_bindex_t checked_bnum;             /* Number of nonlatest branches failed */
	mtfs_bindex_t latest_bnum;              /* Number of latest branches */
	struct mtfs_operation_binfo op_binfo[HRFS_BRANCH_MAX];  /* Global bindex */
	mtfs_bindex_t valid_bnum;               /* Number of valid branches */
	mtfs_bindex_t success_bnum;             /* Number of branches succeeded */
	mtfs_bindex_t success_latest_bnum;      /* Number of latest branches succeeded */
	mtfs_bindex_t success_nonlatest_bnum;   /* Number of nonlatest branches succeeded */
	mtfs_bindex_t fault_bnum;               /* Number of branches failed */
	mtfs_bindex_t fault_latest_bnum;        /* Number of latest branches failed */
	mtfs_bindex_t fault_nonlatest_bnum;     /* Number of nonlatest branches failed */
};

extern struct mtfs_operation_list *mtfs_oplist_build_keep_order(struct inode *inode);
extern struct mtfs_operation_list *mtfs_oplist_build(struct inode *inode);
extern struct mtfs_operation_list *mtfs_oplist_alloc(mtfs_bindex_t bnum);
extern void mtfs_oplist_free(struct mtfs_operation_list *list);
static inline int mtfs_oplist_setbranch(struct mtfs_operation_list *list,
                          mtfs_bindex_t bindex,
                          int is_successful,
                          mtfs_operation_result_t result)
{
	HASSERT(bindex >= 0 && bindex < list->bnum);
	list->op_binfo[bindex].valid = 1;
	list->op_binfo[bindex].is_suceessful = is_successful;
	list->op_binfo[bindex].result = result;
	return 0;
}
mtfs_operation_result_t mtfs_oplist_result(struct mtfs_operation_list *list);
int mtfs_oplist_update(struct inode *inode, struct mtfs_operation_list *list);
int mtfs_oplist_check(struct mtfs_operation_list *list);
#endif /* defined (__linux__) && defined(__KERNEL__) */
#endif /* __HRFS_FLAG_H__ */