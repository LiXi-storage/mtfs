/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_REPLICA_EXT2_H__
#define __MTFS_REPLICA_EXT2_H__
#if defined (__linux__) && defined(__KERNEL__)
#include <mtfs_file.h>
#include <linux/ext2_fs.h>

static inline int ext2_to_inode_flags(int flags)
{
	return (flags & EXT2_RESERVED_FL) ?
	       (((flags & EXT2_SYNC_FL)     ? S_SYNC      : 0) |
	       ((flags & EXT2_NOATIME_FL)   ? S_NOATIME   : 0) |
	       ((flags & EXT2_APPEND_FL)    ? S_APPEND    : 0) |
#if defined(S_DIRSYNC)
	       ((flags & EXT2_DIRSYNC_FL)   ? S_DIRSYNC   : 0) |
#endif
	       ((flags & EXT2_IMMUTABLE_FL) ? S_IMMUTABLE : 0)) :
	       (flags & ~EXT2_RESERVED_FL);
}

static inline int inode_to_ext2_flags(int iflags, int keep)
{
	return keep ? (iflags & ~EXT2_RESERVED_FL) :
	              (((iflags & S_SYNC)     ? EXT2_SYNC_FL      : 0) |
	              ((iflags & S_NOATIME)   ? EXT2_NOATIME_FL   : 0) |
	              ((iflags & S_APPEND)    ? EXT2_APPEND_FL    : 0) |
#if defined(S_DIRSYNC)
	              ((iflags & S_DIRSYNC)   ? EXT2_DIRSYNC_FL   : 0) |
#endif
	              ((iflags & S_IMMUTABLE) ? EXT2_IMMUTABLE_FL : 0));
}

#endif /* defined (__linux__) && defined(__KERNEL__) */
#endif /* __MTFS_REPLICA_EXT2_H__ */
