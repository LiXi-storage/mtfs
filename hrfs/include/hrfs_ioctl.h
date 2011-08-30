/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
 */

#ifndef __HRFS_IOCTL_H__
#define __HRFS_IOCTL_H__
#include <hrfs_types.h>
#include <hrfs_common.h>
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
#define HRFS_IOCTL_GET_DEBUG_LEVEL  _IOR(0x15, 3, HRFS_IOCTL_DATA_TYPE)
#define HRFS_IOCTL_SET_DEBUG_LEVEL  _IOW(0x15, 4, HRFS_IOCTL_DATA_TYPE)
#define HRFS_IOCTL_RULE_ADD         _IOW(0x15, 5, HRFS_IOCTL_DATA_TYPE)
#define HRFS_IOCTL_RULE_DEL         _IOW(0x15, 6, HRFS_IOCTL_DATA_TYPE)
#define HRFS_IOCTL_RULE_LIST        _IOW(0x15, 7, HRFS_IOCTL_DATA_TYPE)

typedef struct hrfs_branch_state {
	__u32 flag;
	__u32 flag1;
} hrfs_branch_state_t;

typedef struct hrfs_user_state {
	int state_size;
	hrfs_bindex_t bnum; /* must be bigger than hrfs_bindex_t */
	hrfs_branch_state_t state[0]; /* per-brach data */
} hrfs_user_state_t;

static inline int hrfs_user_state_size(hrfs_bindex_t bnum)
{
	return sizeof(struct hrfs_user_state) + bnum * sizeof(struct hrfs_branch_state);
}

#endif /* __HRFS_IOCTL_H__ */
