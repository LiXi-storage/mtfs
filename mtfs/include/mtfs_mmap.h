/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_MMAP_H__
#define __MTFS_MMAP_H__

#if defined (__linux__) && defined(__KERNEL__)
extern int mtfs_writepage(struct page *page, struct writeback_control *wbc);
extern int mtfs_readpage(struct file *file, struct page *page);
extern int mtfs_prepare_write(struct file *file, struct page *page, unsigned from, unsigned to);
extern int mtfs_commit_write(struct file *file, struct page *page, unsigned from, unsigned to);
extern struct page *mtfs_nopage(struct vm_area_struct *vma, unsigned long address,
                         int *type);
extern struct address_space_operations mtfs_aops;
extern struct vm_operations_struct mtfs_file_vm_ops;
#else /* !defined (__linux__) && defined(__KERNEL__) */
#error This head is only for kernel space use
#endif /* !defined (__linux__) && defined(__KERNEL__) */

#endif /* __MTFS_MMAP_H__ */
