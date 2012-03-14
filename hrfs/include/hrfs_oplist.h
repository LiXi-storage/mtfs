/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
 */

#ifndef __HRFS_OPLIST_H__
#define __HRFS_OPLIST_H__
#include <hrfs_common.h>
#include <debug.h>
#if defined (__linux__) && defined(__KERNEL__)
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

extern struct hrfs_operation_list *hrfs_oplist_build_keep_order(struct inode *inode);
extern struct hrfs_operation_list *hrfs_oplist_build(struct inode *inode);
extern struct hrfs_operation_list *hrfs_oplist_alloc(hrfs_bindex_t bnum);
extern void hrfs_oplist_free(struct hrfs_operation_list *list);
static inline int hrfs_oplist_setbranch(struct hrfs_operation_list *list,
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
hrfs_operation_result_t hrfs_oplist_result(struct hrfs_operation_list *list);
int hrfs_oplist_update(struct inode *inode, struct hrfs_operation_list *list);
int hrfs_oplist_check(struct hrfs_operation_list *list);
#endif /* defined (__linux__) && defined(__KERNEL__) */
#endif /* __HRFS_FLAG_H__ */
