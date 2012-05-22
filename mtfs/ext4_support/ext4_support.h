/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __HRFS_EXT4_SUPPORT_H__
#define __HRFS_EXT4_SUPPORT_H__
#if defined (__linux__) && defined(__KERNEL__)
#include <mtfs_file.h>

#define EXT4_SUPER_MAGIC 0xEF53
#endif /* defined (__linux__) && defined(__KERNEL__) */
#endif /* __HRFS_EXT4_SUPPORT_H__ */