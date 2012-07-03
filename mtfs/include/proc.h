/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_PROC_H__
#define __MTFS_PROC_H__

#include <linux/sysctl.h>
#include <linux/proc_fs.h>

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

extern void mtfs_proc_remove(struct proc_dir_entry **rooth);
extern struct proc_dir_entry *mtfs_proc_register(const char *name,
                                            struct proc_dir_entry *parent,
                                            struct mtfs_proc_vars *list, void *data);
extern struct ctl_table mtfs_top_table[];
#endif /* __MTFS_PROC_H__ */
