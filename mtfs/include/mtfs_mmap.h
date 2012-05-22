/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __HRFS_MMAP_H__
#define __HRFS_MMAP_H__

extern int mtfs_writepage(struct page *page, struct writeback_control *wbc);
extern int mtfs_readpage(struct file *file, struct page *page);
extern int mtfs_prepare_write(struct file *file, struct page *page, unsigned from, unsigned to);
extern int mtfs_commit_write(struct file *file, struct page *page, unsigned from, unsigned to);
extern ssize_t mtfs_direct_IO(int rw, struct kiocb *kiocb,
						      const struct iovec *iov, loff_t file_offset,
						      unsigned long nr_segs);
extern struct page *mtfs_nopage(struct vm_area_struct *vma, unsigned long address,
                         int *type);
#endif /* __HRFS_MMAP_H__ */