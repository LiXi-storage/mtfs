/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_IOCTL_H__
#define __MTFS_IOCTL_H__
#include <mtfs_types.h>
#include <mtfs_common.h>
#include <mtfs_flag.h>
#if defined(__linux__) && defined(__KERNEL__)
#include <linux/limits.h>
#else /* !defined(__linux__) && defined(__KERNEL__)*/
#include <limits.h>
#endif /* !defined(__linux__) && defined(__KERNEL__)*/
/*
 * DEFINITIONS FOR USER AND KERNEL CODE:
 * (Note: ioctl numbers 1--9 are reserved for fistgen, the rest
 *  are auto-generated automatically based on the user's .fist file.)
 */
#define MTFS_IOCTL_DATA_TYPE        long

#define	MTFS_IOC_GETFLAGS           FS_IOC_GETFLAGS
#define	MTFS_IOC_SETFLAGS           FS_IOC_SETFLAGS
#define	MTFS_IOC_GETVERSION         FS_IOC_GETVERSION
#define	MTFS_IOC_SETVERSION         FS_IOC_SETVERSION
#define MTFS_IOCTL_GET_FLAG         _IOR(0x15, 1, MTFS_IOCTL_DATA_TYPE)
#define MTFS_IOCTL_SET_FLAG         _IOW(0x15, 2, MTFS_IOCTL_DATA_TYPE)
#define MTFS_IOCTL_RULE_ADD         _IOW(0x15, 3, MTFS_IOCTL_DATA_TYPE)
#define MTFS_IOCTL_RULE_DEL         _IOW(0x15, 4, MTFS_IOCTL_DATA_TYPE)
#define MTFS_IOCTL_RULE_LIST        _IOW(0x15, 5, MTFS_IOCTL_DATA_TYPE)
#define MTFS_IOCTL_REMOVE_BRANCH    _IOW(0x15, 6, MTFS_IOCTL_DATA_TYPE)

struct mtfs_branch_flag {
	__u32 flag;
	__u32 flag1;
};

struct mtfs_user_flag {
	int state_size;
	mtfs_bindex_t bnum; /* must be bigger than mtfs_bindex_t */
	struct mtfs_branch_flag state[0]; /* per-brach data */
};

struct mtfs_remove_branch_info {
	char name[PATH_MAX + 1];
	mtfs_bindex_t bindex;
};

static inline int mtfs_user_flag_size(mtfs_bindex_t bnum)
{
	return sizeof(struct mtfs_user_flag) + bnum * sizeof(struct mtfs_branch_flag);
}
#endif /* __MTFS_IOCTL_H__ */
