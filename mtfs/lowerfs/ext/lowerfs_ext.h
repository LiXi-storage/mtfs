/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_REPLICA_EXT_H__
#define __MTFS_REPLICA_EXT_H__
#if defined (__linux__) && defined(__KERNEL__)
#include <linux/jbd.h>
struct buffer_head *_mlowerfs_ext3_bread(handle_t *handle, struct inode *inode,
                                         int block, int create, int *err);
#endif /* defined (__linux__) && defined(__KERNEL__) */
#endif /* __MTFS_REPLICA_EXT_H__ */
