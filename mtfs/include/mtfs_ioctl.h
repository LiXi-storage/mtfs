/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __HRFS_IOCTL_H__
#define __HRFS_IOCTL_H__
#include <mtfs_types.h>
#include <mtfs_common.h>
#include <mtfs_flag.h>
/*
 * DEFINITIONS FOR USER AND KERNEL CODE:
 * (Note: ioctl numbers 1--9 are reserved for fistgen, the rest
 *  are auto-generated automatically based on the user's .fist file.)
 */
#define HRFS_IOCTL_DATA_TYPE        long

#define	HRFS_IOC_GETFLAGS           FS_IOC_GETFLAGS
#define	HRFS_IOC_SETFLAGS           FS_IOC_SETFLAGS
#define	HRFS_IOC_GETVERSION         FS_IOC_GETVERSION
#define	HRFS_IOC_SETVERSION         FS_IOC_SETVERSION
#define HRFS_IOCTL_GET_FLAG         _IOR(0x15, 1, HRFS_IOCTL_DATA_TYPE)
#define HRFS_IOCTL_SET_FLAG         _IOW(0x15, 2, HRFS_IOCTL_DATA_TYPE)
#define HRFS_IOCTL_RULE_ADD         _IOW(0x15, 3, HRFS_IOCTL_DATA_TYPE)
#define HRFS_IOCTL_RULE_DEL         _IOW(0x15, 4, HRFS_IOCTL_DATA_TYPE)
#define HRFS_IOCTL_RULE_LIST        _IOW(0x15, 5, HRFS_IOCTL_DATA_TYPE)
#define HRFS_IOCTL_REMOVE_BRANCH    _IOW(0x15, 6, HRFS_IOCTL_DATA_TYPE)

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
#endif /* __HRFS_IOCTL_H__ */
