/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_STRING_H__
#define __MTFS_STRING_H__

#if defined (__linux__) && defined(__KERNEL__)
#include <linux/string.h>
#else /* !defined (__linux__) && defined(__KERNEL__) */
#include <string.h>
#endif /* !defined (__linux__) && defined(__KERNEL__) */

#endif /* __MTFS_STRING_H__ */
