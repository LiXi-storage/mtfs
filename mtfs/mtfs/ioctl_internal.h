/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_IOCTL_INTERNAL_H__
#define __MTFS_IOCTL_INTERNAL_H__
#include <mtfs_ioctl.h>

#define MTFS_RESERVED_FL         FS_RESERVED_FL /* reserved for ext2 lib */
#define MTFS_SYNC_FL             FS_SYNC_FL /* Synchronous updates */
#define MTFS_IMMUTABLE_FL        FS_IMMUTABLE_FL /* Immutable file */
#define MTFS_APPEND_FL           FS_APPEND_FL /* writes to file may only append */
#define MTFS_NOATIME_FL          FS_NOATIME_FL /* do not update atime */
#define MTFS_DIRSYNC_FL          FS_DIRSYNC_FL /* dirsync behaviour (dir only) */

static inline int mtfs_lowerfs_to_inode_flags(int flags)
{
	return (flags & MTFS_RESERVED_FL) ?
	       (((flags & MTFS_SYNC_FL) ? S_SYNC : 0) |
	       ((flags & MTFS_NOATIME_FL) ? S_NOATIME : 0) |
	       ((flags & MTFS_APPEND_FL) ? S_APPEND : 0) |
#if defined(S_DIRSYNC)
	       ((flags & MTFS_DIRSYNC_FL) ? S_DIRSYNC : 0) |
#endif
	       ((flags & MTFS_IMMUTABLE_FL) ? S_IMMUTABLE : 0)) :
	       (flags & ~MTFS_RESERVED_FL);
}

static inline int mtfs_inode_to_lowerfs_flags(int iflags, int keep)
{
	return keep ? (iflags & ~MTFS_RESERVED_FL) :
	              (((iflags & S_SYNC)     ? MTFS_SYNC_FL      : 0) |
	              ((iflags & S_NOATIME)   ? MTFS_NOATIME_FL   : 0) |
	              ((iflags & S_APPEND)    ? MTFS_APPEND_FL    : 0) |
#if defined(S_DIRSYNC)
	              ((iflags & S_DIRSYNC)   ? MTFS_DIRSYNC_FL   : 0) |
#endif
	              ((iflags & S_IMMUTABLE) ? MTFS_IMMUTABLE_FL : 0));
}

#endif /* __MTFS_IOCTL_INTERNAL_H__ */
