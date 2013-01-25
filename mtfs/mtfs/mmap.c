/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <linux/module.h>
#include <linux/uio.h>
#include <linux/aio.h>
#include <mtfs_oplist.h>
#include <mtfs_device.h>
#include <mtfs_inode.h>
#include <mtfs_file.h>
#include <mtfs_flag.h>
#include <mtfs_io.h>
#include <mtfs_stack.h>
#include "mmap_internal.h"
#include "main_internal.h"
#include "lowerfs_internal.h"
#include "file_internal.h"

int mtfs_writepage_branch(struct page *page,
                          struct writeback_control *wbc,
                          mtfs_bindex_t bindex)
{
	int ret = 0;
	struct inode *inode = page->mapping->host;
	struct inode *hidden_inode = NULL;
	struct page *hidden_page = NULL;
	char *kaddr = NULL;
	char *lower_kaddr = NULL;
	unsigned hidden_page_index = page->index;
	MENTRY();

	ret = mtfs_device_branch_errno(mtfs_i2dev(inode), bindex, BOPS_MASK_WRITE);
	if (ret) {
		MDEBUG("branch[%d] is abandoned\n", bindex);
		goto out; 
	}

	hidden_inode = mtfs_i2branch(inode, bindex);
	if (hidden_inode == NULL) {
		ret = -ENOENT;
		goto out;
	}

	/* This will lock_page */
	hidden_page = grab_cache_page(hidden_inode->i_mapping, hidden_page_index);
	if (!hidden_page) {
		ret = -ENOMEM; /* Which errno*/
		goto out;
	}

	/*
	 * Writepage is not allowed when writeback is in progress.
	 * And PageWriteback flag will be set by writepage in some fs.
	 * So wait until it finishs.
	 */
	wait_on_page_writeback(hidden_page);
	MASSERT(!PageWriteback(hidden_page));

	/* Copy data to hidden_page */
	kaddr = kmap(page);
	lower_kaddr = kmap(hidden_page);
	memcpy(lower_kaddr, kaddr, PAGE_CACHE_SIZE);
	flush_dcache_page(hidden_page);
	kunmap(hidden_page);
	kunmap(page);

	set_page_dirty(hidden_page);

	if (!clear_page_dirty_for_io(hidden_page)) {
		MERROR("page(%p) is not dirty\n", hidden_page);
		unlock_page(hidden_page);
		page_cache_release(hidden_page);
		MBUG();
		goto out;
	}

	/* This will unlock_page */
	MASSERT(hidden_inode->i_mapping);
	MASSERT(hidden_inode->i_mapping->a_ops);
	MASSERT(hidden_inode->i_mapping->a_ops->writepage);
	ret = hidden_inode->i_mapping->a_ops->writepage(hidden_page, wbc);
	if (ret == AOP_WRITEPAGE_ACTIVATE) {
		MDEBUG("lowerfs choose to not start writepage\n");
		unlock_page(hidden_page);
		ret = 0;
	}

	page_cache_release(hidden_page); /* because read_cache_page increased refcnt */
out:
	MRETURN(ret);
}

int mtfs_writepage(struct page *page, struct writeback_control *wbc)
{
	int ret = 0;
	struct mtfs_io *io = NULL;
	struct mtfs_io_writepage *io_writepage = NULL;
	struct inode *inode = page->mapping->host;
	MENTRY();

	MDEBUG("writepage\n");

	MTFS_SLAB_ALLOC_PTR(io, mtfs_io_cache);
	if (io == NULL) {
		MERROR("not enough memory\n");
		ret = -ENOMEM;
		goto out;
	}

	io_writepage = &io->u.mi_writepage;

	io->mi_type = MIOT_WRITEPAGE;
	io->mi_bindex = 0;
	io->mi_oplist_inode = inode;
	io->mi_bnum = mtfs_i2bnum(inode);
	io->mi_break = 0;
	io->mi_ops = &((*(mtfs_i2ops(inode)->io_ops))[io->mi_type]);

	io_writepage->page = page;
	io_writepage->wbc = wbc;

	ret = mtfs_io_loop(io);
	if (ret) {
		MERROR("failed to loop on io\n");
	} else {
		ret = io->mi_result.ret;
	}

	if (ret) {
		ClearPageUptodate(page);
	} else {
		SetPageUptodate(page);
	}

	MTFS_SLAB_FREE_PTR(io, mtfs_io_cache);
out:
	unlock_page(page);
	MRETURN(ret);
}
EXPORT_SYMBOL(mtfs_writepage);

struct page *mtfs_read_cache_page(struct file *file,
                                  struct inode *hidden_inode,
                                  mtfs_bindex_t bindex,
                                  unsigned page_index)
{
	struct page *hidden_page = NULL;
	int err = 0;

	hidden_page = read_cache_page(hidden_inode->i_mapping, page_index, 
			(filler_t *) hidden_inode->i_mapping->a_ops->readpage,
			(void *) file);

	if (IS_ERR(hidden_page)) {
		goto out;
	}

	if (!PageUptodate(hidden_page)) {
		page_cache_release(hidden_page);
		err = -EIO;
		hidden_page = ERR_PTR(err);
		goto out;
	}

out:
	return hidden_page;
}

int mtfs_readpage_branch(struct file *file,
                         struct page *page,
                         mtfs_bindex_t bindex)
{
	int ret = 0;
	struct dentry *dentry = NULL;
	struct file *hidden_file = NULL;
	struct inode *inode = NULL;
	struct inode *hidden_inode = NULL;
	struct page *hidden_page = NULL;
	unsigned hidden_page_index = page->index;
	void *page_data = NULL;
	MENTRY();

	dentry = file->f_dentry;
	inode = dentry->d_inode;

	MASSERT(mtfs_f2info(file));
	hidden_file = mtfs_f2branch(file, bindex);
	hidden_inode = mtfs_i2branch(inode, bindex);

	hidden_page = mtfs_read_cache_page(hidden_file,
	                                   hidden_inode,
	                                   bindex,
	                                   hidden_page_index);
	if (IS_ERR(hidden_page)) {
		ret = PTR_ERR(hidden_page);
		goto out;
	}

	page_data = kmap(page);
	copy_page_data(page_data, hidden_page, hidden_inode, hidden_page_index);
	page_cache_release(hidden_page);
	kunmap(page);

out:
	MRETURN(ret);
}

int mtfs_readpage(struct file *file, struct page *page)
{
	int ret = 0;
	struct mtfs_io *io = NULL;
	struct mtfs_io_readpage *io_readpage = NULL;
	struct inode *inode = page->mapping->host;
	MENTRY();

	MDEBUG("readpage\n");

	MTFS_SLAB_ALLOC_PTR(io, mtfs_io_cache);
	if (io == NULL) {
		MERROR("not enough memory\n");
		ret = -ENOMEM;
		goto out;
	}

	io_readpage = &io->u.mi_readpage;

	io->mi_type = MIOT_READPAGE;
	io->mi_bindex = 0;
	io->mi_oplist_inode = inode;
	io->mi_bnum = mtfs_i2bnum(inode);
	io->mi_break = 0;
	io->mi_ops = &((*(mtfs_i2ops(inode)->io_ops))[io->mi_type]);

	io_readpage->file = file;
	io_readpage->page = page;

	ret = mtfs_io_loop(io);
	if (ret) {
		MERROR("failed to loop on io\n");
	} else {
		ret = io->mi_result.ret;
	}

	if (ret) {
		ClearPageUptodate(page);
	} else {
		SetPageUptodate(page);
	}

	MTFS_SLAB_FREE_PTR(io, mtfs_io_cache);
out:
	unlock_page(page);
	MRETURN(ret);
}
EXPORT_SYMBOL(mtfs_readpage);

ssize_t mtfs_direct_IO_nop(int rw, struct kiocb *kiocb,
                       const struct iovec *iov, loff_t file_offset,
                       unsigned long nr_segs)
{
	ssize_t ret = 0;
	MENTRY();

	MERROR("should never come here\n");
	MBUG();

	MRETURN(ret);
}

#ifdef HAVE_VM_OP_FAULT
int mtfs_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	int ret = 0;
	MENTRY();

	ret = filemap_fault(vma, vmf);
	MRETURN(ret);
}
EXPORT_SYMBOL(mtfs_fault);
#else /* ! HAVE_VM_OP_FAULT */
struct page *mtfs_nopage(struct vm_area_struct *vma, unsigned long address,
                         int *type)
{
	struct page *page = NULL;
	MENTRY();

	page = filemap_nopage(vma, address, type);
	if (page == NOPAGE_SIGBUS || page == NOPAGE_OOM) {
		MERROR("got addr %lu type %lx - SIGBUS\n", address, (long)type);
	}
	MRETURN(page);
}
EXPORT_SYMBOL(mtfs_nopage);
#endif /* ! HAVE_VM_OP_FAULT */

struct address_space_operations mtfs_aops =
{
	direct_IO:      mtfs_direct_IO_nop,
	writepage:      mtfs_writepage,
	readpage:       mtfs_readpage,
};
EXPORT_SYMBOL(mtfs_aops);

struct vm_operations_struct mtfs_file_vm_ops = {
#ifdef HAVE_VM_OP_FAULT
	fault:          mtfs_fault,
#else /* ! HAVE_VM_OP_FAULT */
	nopage:         mtfs_nopage,
	populate:       filemap_populate,
#endif /* ! HAVE_VM_OP_FAULT */
};
EXPORT_SYMBOL(mtfs_file_vm_ops);
