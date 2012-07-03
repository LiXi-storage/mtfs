/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */
#ifndef __MTFS_THREAD_H__
#define __MTFS_THREAD_H__

#if defined (__linux__) && defined(__KERNEL__)
extern int mtfs_create_thread(int (*fn)(void *), void *arg, unsigned long flags)
#else /* defined (__linux__) && defined(__KERNEL__) */
#error This head is only for kernel space use
#endif /* ! defined (__linux__) && defined(__KERNEL__) */

#endif /*__MTFS_THREAD_H__ */
