/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
 */

#ifndef __HRFS_TYPES_H__
#define __HRFS_TYPES_H__
#if defined (__linux__) && defined(__KERNEL__)
#include <linux/types.h> /* For size_t */
#else /* !(defined (__linux__) && defined(__KERNEL__)) */
#include <sys/types.h>
#include <linux/types.h>
#endif /* !(defined (__linux__) && defined(__KERNEL__)) */
#endif /* __HRFS_TYPES_H__ */
