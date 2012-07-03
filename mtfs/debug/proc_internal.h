/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_PROC_INTERNAL_H__
#define __MTFS_PROC_INTERNAL_H__

#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/sysctl.h>
#include <proc.h>

#ifdef HAVE_SYSCTL_UNNUMBERED
#define CTL_MTFS              CTL_UNNUMBERED
#define MTFS_PROC_DEBUG       CTL_UNNUMBERED
#define MTFS_PROC_MEMUSED     CTL_UNNUMBERED
#define MTFS_PROC_MEMUSED_MAX CTL_UNNUMBERED
#define MTFS_PROC_DUMP_KERNEL CTL_UNNUMBERED
#define MTFS_PROC_DEBUG_MASK  CTL_UNNUMBERED
#else /* !define(HAVE_SYSCTL_UNNUMBERED) */
#define CTL_MTFS         (0x100)
enum {
	MTFS_PROC_DEBUG = 1, /* control debugging */
	MTFS_PROC_MEMUSED,   /* bytes currently allocated */
	MTFS_PROC_MEMUSED_MAX,
	MTFS_PROC_DUMP_KERNEL,
	MTFS_PROC_DEBUG_MASK,
};
#endif /* !define(HAVE_SYSCTL_UNNUMBERED) */

#ifdef HAVE_5ARGS_SYSCTL_PROC_HANDLER
#define MTFS_PROC_PROTO(name)                                           \
        name(struct ctl_table *table, int write,                      \
             void __user *buffer, size_t *lenp, loff_t *ppos)
#else /* !HAVE_5ARGS_SYSCTL_PROC_HANDLER */
#define MTFS_PROC_PROTO(name)                                           \
        name(struct ctl_table *table, int write, struct file *filp,   \
             void __user *buffer, size_t *lenp, loff_t *ppos)
#endif /* !HAVE_5ARGS_SYSCTL_PROC_HANDLER */

#define MTFS_DECLARE_PROC_PPOS_DECL

#define MTFS_DECLARE_PROC_HANDLER(name)                 \
static int                                              \
MTFS_PROC_PROTO(name)                                   \
{                                                       \
        MTFS_DECLARE_PROC_PPOS_DECL;                    \
                                                        \
        return mtfs_proc_call_handler(table->data, write,    \
                                 ppos, buffer, lenp,    \
                                 __##name);             \
}
#endif /* __MTFS_PROC_INTERNAL_H__ */
