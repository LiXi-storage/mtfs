/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_OPLIST_H__
#define __MTFS_OPLIST_H__

#if defined (__linux__) && defined(__KERNEL__)
#include <linux/types.h>
#else /* !defined (__linux__) && defined(__KERNEL__) */
#endif /* !defined (__linux__) && defined(__KERNEL__) */

typedef union mtfs_operation_result {
	int ret;
	void *ptr;
	ssize_t ssize;
} mtfs_operation_result_t;

#if defined (__linux__) && defined(__KERNEL__)
#include <linux/fs.h>
#include "mtfs_common.h"
#include "debug.h"

struct mtfs_operation_binfo {
	mtfs_bindex_t bindex;     /* Global bindex */
	int is_suceessful;        /* Whether this branch succeeded */
	mtfs_operation_result_t result;
	int valid;
};

struct mtfs_operation_list;

struct mtfs_oplist_object {
	int  (* mopo_init)(struct mtfs_operation_list *oplist, struct inode *inode);  /* Init oplist */
	int  (* mopo_flush)(struct mtfs_operation_list *oplist, struct inode *inode); /* Flush oplist */
	int  (* mopo_gather)(struct mtfs_operation_list *oplist);                     /* Gather oplist */
};

struct mtfs_operation_list {
	int           inited;                   /* Inited or not */
	mtfs_bindex_t bnum;                     /* Branch number */
	mtfs_bindex_t latest_bnum;              /* Number of latest branches */
	struct mtfs_operation_binfo op_binfo[MTFS_BRANCH_MAX];  /* Global bindex */
	struct mtfs_operation_binfo *opinfo;    /* Merged Result */
	mtfs_bindex_t valid_bnum;               /* Number of valid branches */
	mtfs_bindex_t success_bnum;             /* Number of branches succeeded */
	mtfs_bindex_t success_latest_bnum;      /* Number of latest branches succeeded */
	mtfs_bindex_t success_nonlatest_bnum;   /* Number of nonlatest branches succeeded */
	mtfs_bindex_t fault_bnum;               /* Number of branches failed */
	mtfs_bindex_t fault_latest_bnum;        /* Number of latest branches failed */
	mtfs_bindex_t fault_nonlatest_bnum;     /* Number of nonlatest branches failed */
	struct mtfs_oplist_object *mopl_type;
};

extern struct mtfs_operation_list *mtfs_oplist_build(struct inode *inode,
                                                     struct mtfs_oplist_object *oplist_obj);
extern void mtfs_oplist_free(struct mtfs_operation_list *oplist);
extern int mtfs_oplist_init(struct mtfs_operation_list *oplist,
                            struct inode *inode,
                            struct mtfs_oplist_object *oplist_obj);
extern int mtfs_oplist_flush(struct mtfs_operation_list *oplist,
                             struct inode *inode);
extern int mtfs_oplist_gather(struct mtfs_operation_list *oplist);
extern int mtfs_oplist_setbranch(struct mtfs_operation_list *oplist,
                                 mtfs_bindex_t bindex,
                                 int is_successful,
                                 mtfs_operation_result_t result);
extern mtfs_operation_result_t mtfs_oplist_result(struct mtfs_operation_list *list);
extern struct mtfs_oplist_object mtfs_oplist_flag;
extern struct mtfs_oplist_object mtfs_oplist_sequential;
extern struct mtfs_oplist_object mtfs_oplist_master;
extern struct mtfs_oplist_object mtfs_oplist_equal;
#endif /* defined (__linux__) && defined(__KERNEL__) */

#endif /* __MTFS_FLAG_H__ */
