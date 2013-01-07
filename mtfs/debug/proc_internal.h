/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_PROC_INTERNAL_H__
#define __MTFS_PROC_INTERNAL_H__

#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/sysctl.h>
#include <mtfs_proc.h>

#ifdef HAVE_SYSCTL_UNNUMBERED
#define CTL_MTFS                CTL_UNNUMBERED
#define MTFS_PROC_DEBUG         CTL_UNNUMBERED
#define MTFS_PROC_MEMUSED       CTL_UNNUMBERED
#define MTFS_PROC_MEMUSED_MAX   CTL_UNNUMBERED
#define MTFS_PROC_DUMP_KERNEL   CTL_UNNUMBERED
#define MTFS_PROC_DEBUG_MASK    CTL_UNNUMBERED
#define MTFS_PROC_CATASTROPHE   CTL_UNNUMBERED
#define MTFS_PROC_PANIC_ON_MBUG CTL_UNNUMBERED
#else /* !define(HAVE_SYSCTL_UNNUMBERED) */
#define CTL_MTFS         (0x100)
enum {
	MTFS_PROC_DEBUG = 1, /* control debugging */
	MTFS_PROC_MEMUSED,   /* bytes currently allocated */
	MTFS_PROC_MEMUSED_MAX,
	MTFS_PROC_DUMP_KERNEL,
	MTFS_PROC_DEBUG_MASK,
	MTFS_PROC_CATASTROPHE,
	MTFS_PROC_PANIC_ON_MBUG,
};
#endif /* !define(HAVE_SYSCTL_UNNUMBERED) */

#endif /* __MTFS_PROC_INTERNAL_H__ */
