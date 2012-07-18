/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_MMAP_INTERNAL_H__
#define __MTFS_MMAP_INTERNAL_H__

#include <linux/pagemap.h>
#include <mtfs_mmap.h>

extern struct address_space_operations mtfs_aops;
extern struct vm_operations_struct mtfs_file_vm_ops;
struct page *mtfs_read_cache_page(struct file *file, struct inode *hidden_inode, mtfs_bindex_t bindex, unsigned page_index);

static inline int __copy_page_data(void *broken_page_data, void *correct_page_data, struct inode *correct_inode, unsigned long hidden_page_index)
{
	loff_t real_size = 0;
	int ret = 0;
	
	real_size = correct_inode->i_size - (hidden_page_index << PAGE_CACHE_SHIFT);
	if (real_size <= 0) {
		memset(broken_page_data, 0, PAGE_CACHE_SIZE);
	} else if (real_size < PAGE_CACHE_SIZE) {
		memcpy(broken_page_data, correct_page_data, real_size);
		memset(broken_page_data + real_size, 0, PAGE_CACHE_SIZE - real_size);
	} else {
		memcpy(broken_page_data, correct_page_data, PAGE_CACHE_SIZE);
	}
	return ret;
}

static inline int copy_page_data(void *broken_page_data, struct page *correct_page, struct inode *correct_inode, unsigned long hidden_page_index)
{
	void *correct_page_data = NULL;	
	int ret = 0;
	
	correct_page_data = kmap(correct_page);
	__copy_page_data(broken_page_data, correct_page_data, correct_inode, hidden_page_index);
	kunmap(correct_page);
	
	return ret;
}
#endif /* __MTFS_MMAP_INTERNAL_H__ */
