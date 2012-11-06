/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_PROC_H__
#define __MTFS_PROC_H__

#if defined(__linux__) && defined(__KERNEL__)

#include <linux/proc_fs.h>
#include <linux/sysctl.h>

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

#define MTFS_DECLARE_PROC_HANDLER(name)                   \
static int                                                \
MTFS_PROC_PROTO(name)                                     \
{                                                         \
        MTFS_DECLARE_PROC_PPOS_DECL;                      \
                                                          \
        return mtfs_proc_call_handler(table->data, write, \
                                 ppos, buffer, lenp,      \
                                 __##name);               \
}
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

extern int mtfs_trace_copyin_string(char *knl_buffer, int knl_buffer_nob,
                                    const char *usr_buffer, int usr_buffer_nob);
extern int mtfs_common_str2mask(int *mask,
                                const char *str,
                                const char *(*mask2str)(int bit),
                                int minmask);
extern int mtfs_common_mask2str(char *str,
                                int size,
                                int mask,
                                const char *(*mask2str)(int bit));
extern void mtfs_proc_remove(struct proc_dir_entry **rooth);
extern struct proc_dir_entry *mtfs_proc_register(const char *name,
                                            struct proc_dir_entry *parent,
                                            struct mtfs_proc_vars *list, void *data);
extern struct ctl_table mtfs_top_table[];
extern int mtfs_common_proc_dobitmasks(void *data, int write,
                                       loff_t pos, void *buffer,
                                       int nob, int minmask,
                                       char *tmpstr, int tmpstrlen,
                                       const char *(*mask2str)(int bit));
extern int
mtfs_proc_call_handler(void *data, int write,
                       loff_t *ppos, void *buffer, size_t *lenp,
                       int (*handler)(void *data, int write,
                       loff_t pos, void *buffer, int len));
#else /* !defined (__linux__) && defined(__KERNEL__) */
#error This head is only for kernel space use
#endif /* !defined (__linux__) && defined(__KERNEL__) */

#endif /* __MTFS_PROC_H__ */
