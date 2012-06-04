/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_TYPES_H__
#define __MTFS_TYPES_H__
#if defined(__linux__) && defined(__KERNEL__)
#include <linux/types.h> /* For size_t */
#else /* !(defined (__linux__) && defined(__KERNEL__)) */
#include <sys/types.h>
#endif /* !(defined (__linux__) && defined(__KERNEL__)) */
#endif /* __MTFS_TYPES_H__ */
