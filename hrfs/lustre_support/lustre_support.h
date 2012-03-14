/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
 */

#ifndef __HRFS_LUSTRE_SUPPORT_H__
#define __HRFS_LUSTRE_SUPPORT_H__
#if defined (__linux__) && defined(__KERNEL__)

#include <lustre/lustre_user.h>
#define LUSTRE_SUPER_MAGIC 0x0BD00BD1
#include <hrfs_oplist.h>
int hrfs_ll_file_writev(struct file *hrfs_file, const struct iovec *iov,
                            unsigned long nr_segs, loff_t *ppos,
                            struct hrfs_operation_list *oplist);
extern struct vm_operations_struct *hrfs_ll_get_vm_ops(void);
extern int hrfs_ll_inode_get_flag(struct inode *inode, __u32 *hrfs_flag);
extern int hrfs_ll_inode_set_flag(struct inode *inode, __u32 hrfs_flag);
#endif /* defined (__linux__) && defined(__KERNEL__) */
#endif /* __HRFS_LUSTRE_SUPPORT_H__ */
