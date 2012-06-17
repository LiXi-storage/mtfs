/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_PROC_INTERNAL_H__
#define __MTFS_PROC_INTERNAL_H__

#include <linux/proc_fs.h>

int mtfs_insert_proc(void);
void mtfs_remove_proc(void);
void mtfs_proc_remove(struct proc_dir_entry **rooth);

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

struct mtfs_proc_vars {
        const char   *name;
        read_proc_t *read_fptr;
        write_proc_t *write_fptr;
        void *data;
        struct file_operations *fops;
        /**
         * /proc file mode.
         */
        mode_t proc_mode;
};

struct proc_dir_entry *mtfs_proc_register(const char *name,
                                            struct proc_dir_entry *parent,
                                            struct mtfs_proc_vars *list, void *data);
extern struct proc_dir_entry *mtfs_proc_root;
extern struct proc_dir_entry *mtfs_proc_device;


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
