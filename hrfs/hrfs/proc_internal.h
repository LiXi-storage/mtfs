/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
 */

#ifndef __HRFS_PROC_INTERNAL_H__
#define __HRFS_PROC_INTERNAL_H__
int hrfs_insert_proc(void);
void hrfs_remove_proc(void);

#ifdef HAVE_SYSCTL_UNNUMBERED
#define CTL_HRFS          CTL_UNNUMBERED
#define HRFS_PROC_DEBUG   CTL_UNNUMBERED
#define HRFS_PROC_MEMUSED CTL_UNNUMBERED
#else /* !define(HAVE_SYSCTL_UNNUMBERED) */
#define CTL_HRFS         (0x100)
enum {
	HRFS_PROC_DEBUG = 1, /* control debugging */
	HRFS_PROC_MEMUSED,   /* bytes currently allocated */
};
#endif /* !define(HAVE_SYSCTL_UNNUMBERED) */
#endif /* __HRFS_PROC_INTERNAL_H__ */
