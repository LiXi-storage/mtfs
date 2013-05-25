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

#define MTFS_RESERVE_ROOT    ".mtfs"
#define MTFS_RESERVE_RECOVER "RECOVER"
#define MTFS_RESERVE_CONFIG  "CONFIG"
#define MTFS_RESERVE_LOG     "LOG"

#ifndef MIN
# define MIN(a,b) (((a)<(b)) ? (a): (b))
#endif
#ifndef MAX
# define MAX(a,b) (((a)>(b)) ? (a): (b))
#endif

static inline int size_round4(int val)
{
	return (val + 3) & (~0x3);
}

static inline int size_round(int val)
{
	return (val + 7) & (~0x7);
}

static inline int size_round16(int val)
{
	return (val + 0xf) & (~0xf);
}

static inline int size_round32(int val)
{
	return (val + 0x1f) & (~0x1f);
}

static inline int size_round0(int val)
{
	if (!val)
		return 0;
	return (val + 1 + 7) & (~0x7);
}

#endif /* __MTFS_COMMON_H__ */
