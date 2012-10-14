/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_COMMON_H__
#define __MTFS_COMMON_H__

#define MTFS_BRANCH_MAX 8
typedef signed char mtfs_bindex_t;

#define MTFS_HIDDEN_FS_TYPE "mtfs-hidden"
#define MTFS_FS_TYPE "mtfs"
#define MTFS_HIDDEN_FS_DEV "none"

#define MTFS_DEFAULT_PRIMARY_BRANCH 0
#define MTFS_DEFAULT_SUBJECT "REPLICA"

static inline int mtfs_is_primary_bindex(mtfs_bindex_t bindex)
{
	return bindex == MTFS_DEFAULT_PRIMARY_BRANCH;
}

static inline mtfs_bindex_t mtfs_get_primary_bindex(void)
{
	return MTFS_DEFAULT_PRIMARY_BRANCH;
}

#ifndef MIN
# define MIN(a,b) (((a)<(b)) ? (a): (b))
#endif
#ifndef MAX
# define MAX(a,b) (((a)>(b)) ? (a): (b))
#endif
#endif /* __MTFS_COMMON_H__ */
