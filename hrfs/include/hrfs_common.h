/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __HRFS_COMMON_H__
#define __HRFS_COMMON_H__

#define HRFS_BRANCH_MAX 8
typedef signed char hrfs_bindex_t;

#define HRFS_HIDDEN_FS_TYPE "hrfs-hidden"
#define HRFS_FS_TYPE "hrfs"
#define HRFS_HIDDEN_FS_DEV "none"

#define HRFS_DEFAULT_PRIMARY_BRANCH 0

static inline int hrfs_is_primary_bindex(hrfs_bindex_t bindex)
{
	return bindex == HRFS_DEFAULT_PRIMARY_BRANCH;
}

static inline hrfs_bindex_t hrfs_get_primary_bindex(void)
{
	return HRFS_DEFAULT_PRIMARY_BRANCH;
}
#endif /* __HRFS_COMMON_H__ */
