/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __HRFS_IOCTL_INTERNAL_H__
#define __HRFS_IOCTL_INTERNAL_H__
#include <hrfs_ioctl.h>

#define HRFS_RESERVED_FL         FS_RESERVED_FL /* reserved for ext2 lib */
#define HRFS_SYNC_FL             FS_SYNC_FL /* Synchronous updates */
#define HRFS_IMMUTABLE_FL        FS_IMMUTABLE_FL /* Immutable file */
#define HRFS_APPEND_FL           FS_APPEND_FL /* writes to file may only append */
#define HRFS_NOATIME_FL          FS_NOATIME_FL /* do not update atime */
#define HRFS_DIRSYNC_FL          FS_DIRSYNC_FL /* dirsync behaviour (dir only) */

static inline int hrfs_lowerfs_to_inode_flags(int flags)
{
	return (flags & HRFS_RESERVED_FL) ?
	       (((flags & HRFS_SYNC_FL) ? S_SYNC : 0) |
	       ((flags & HRFS_NOATIME_FL) ? S_NOATIME : 0) |
	       ((flags & HRFS_APPEND_FL) ? S_APPEND : 0) |
#if defined(S_DIRSYNC)
	       ((flags & HRFS_DIRSYNC_FL) ? S_DIRSYNC : 0) |
#endif
	       ((flags & HRFS_IMMUTABLE_FL) ? S_IMMUTABLE : 0)) :
	       (flags & ~HRFS_RESERVED_FL);
}

static inline int hrfs_inode_to_lowerfs_flags(int iflags, int keep)
{
	return keep ? (iflags & ~HRFS_RESERVED_FL) :
	              (((iflags & S_SYNC)     ? HRFS_SYNC_FL      : 0) |
	              ((iflags & S_NOATIME)   ? HRFS_NOATIME_FL   : 0) |
	              ((iflags & S_APPEND)    ? HRFS_APPEND_FL    : 0) |
#if defined(S_DIRSYNC)
	              ((iflags & S_DIRSYNC)   ? HRFS_DIRSYNC_FL   : 0) |
#endif
	              ((iflags & S_IMMUTABLE) ? HRFS_IMMUTABLE_FL : 0));
}

#endif /* __HRFS_IOCTL_INTERNAL_H__ */
