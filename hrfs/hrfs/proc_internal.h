/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
 */

#ifndef __HRFS_PROC_INTERNAL_H__
#define __HRFS_PROC_INTERNAL_H__
int hrfs_insert_proc(void);
void hrfs_remove_proc(void);
void hrfs_proc_remove(struct proc_dir_entry **rooth);

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

struct hrfs_proc_vars {
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

struct proc_dir_entry *hrfs_proc_register(const char *name,
                                            struct proc_dir_entry *parent,
                                            struct hrfs_proc_vars *list, void *data);
extern struct proc_dir_entry *hrfs_proc_root;
extern struct proc_dir_entry *hrfs_proc_device;
#endif /* __HRFS_PROC_INTERNAL_H__ */
