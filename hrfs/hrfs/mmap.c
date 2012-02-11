/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
 */

#include "hrfs_internal.h"
#include "mmap_internal.h"
#include <linux/uio.h>

static int hrfs_writepage_branch(struct page *page, struct inode *inode, hrfs_bindex_t bindex,
                                 unsigned hidden_page_index, struct writeback_control *wbc)
{
	int ret = 0;
	struct inode *lower_inode = NULL;
	struct page *lower_page;
	HENTRY();

	ret = hrfs_device_branch_errno(hrfs_i2dev(inode), bindex, BOPS_MASK_WRITE);
	if (ret) {
		HDEBUG("branch[%d] is abandoned\n", bindex);
		goto out; 
	}

	lower_inode = hrfs_i2branch(inode, bindex);
	if (lower_inode == NULL) {
		ret = -ENOENT;
		goto out;
	}

	/* This will lock_page */
	lower_page = grab_cache_page(lower_inode->i_mapping, hidden_page_index);
	if (!lower_page) {
		ret = -ENOMEM;
		goto out;
	}

	set_page_dirty(lower_page);
	if (wbc->sync_mode != WB_SYNC_NONE) {
		wait_on_page_writeback(lower_page);
	}

	if (PageWriteback(lower_page) || !clear_page_dirty_for_io(lower_page)) {
		unlock_page(lower_page);
		page_cache_release(lower_page);
		LBUG();
		goto out;
	}

	SetPageReclaim(lower_page);

	/* This will unlock_page*/
	ret = lower_inode->i_mapping->a_ops->writepage(lower_page, wbc);
	if (ret) {
		ClearPageReclaim(lower_page);
		page_cache_release(lower_page);
		goto out;
	}
	
	if (!PageWriteback(lower_page)) {
		ClearPageReclaim(lower_page);
	}

	page_cache_release(lower_page); /* because read_cache_page increased refcnt */
out:
	HRETURN(ret);
}

int hrfs_writepage(struct page *page, struct writeback_control *wbc)
{
	int ret = 0;
	struct inode *inode = page->mapping->host;
	hrfs_bindex_t i = 0;
	hrfs_bindex_t bindex = 0;
	struct hrfs_operation_list *list = NULL;
	hrfs_operation_result_t result = {0};
	HENTRY();

	list = hrfs_oplist_build(inode);
	if (unlikely(list == NULL)) {
		HERROR("failed to build operation list\n");
		ret = -ENOMEM;
		goto out;
	}

	if (list->latest_bnum == 0) {
		HERROR("page has no valid branch, please check it\n");
		if (!(hrfs_i2dev(inode)->no_abort)) {
			ret = -EIO;
			goto out_free_oplist;
		}
	}

	for (i = 0; i < hrfs_i2bnum(inode); i++) {
		bindex = list->op_binfo[i].bindex;
		ret = hrfs_writepage_branch(page, inode, bindex, page->index, wbc);
		result.ret = ret;
		hrfs_oplist_setbranch(list, i, (ret == 0 ? 1 : 0), result);
		if (i == list->latest_bnum - 1) {
			hrfs_oplist_check(list);
			if (list->success_latest_bnum <= 0) {
				HDEBUG("operation failed for all latest branches\n");
				if (!(hrfs_i2dev(inode)->no_abort)) {
					ret = -EIO;
					goto out_free_oplist;
				}
			}
		}
	}

	hrfs_oplist_check(list);
	if (list->success_bnum <= 0) {
		result = hrfs_oplist_result(list);
		ret = result.size;
		goto out_free_oplist;
	}

	ret = hrfs_oplist_update(inode, list);
	if (ret) {
		HERROR("failed to update inode\n");
		HBUG();
	}

	if (ret) {
		ClearPageUptodate(page);
	} else {
		SetPageUptodate(page);
	}

out_free_oplist:
	hrfs_oplist_free(list);
out:
	unlock_page(page);
	HRETURN(ret);
}
EXPORT_SYMBOL(hrfs_writepage);

struct page *hrfs_read_cache_page(struct file *file, struct inode *hidden_inode, hrfs_bindex_t bindex, unsigned page_index)
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

static int hrfs_readpage_branch(struct file *file, char *page_data, hrfs_bindex_t bindex, unsigned hidden_page_index)
{
	int ret = 0;
	struct dentry *dentry = NULL;
	struct file *hidden_file = NULL;
	struct inode *inode = NULL;
	struct inode *hidden_inode = NULL;
	struct page *hidden_page = NULL;
	HENTRY();

	dentry = file->f_dentry;
	inode = dentry->d_inode;

	HASSERT(hrfs_f2info(file));
	hidden_file = hrfs_f2branch(file, bindex);
	hidden_inode = hrfs_i2branch(inode, bindex);

	hidden_page = hrfs_read_cache_page(hidden_file, hidden_inode, bindex, hidden_page_index);
	if (IS_ERR(hidden_page)) {
		ret = PTR_ERR(hidden_page);
		goto out;
	}

	copy_page_data(page_data, hidden_page, hidden_inode, hidden_page_index);
	page_cache_release(hidden_page);

out:
	HRETURN(ret);
}

static int hrfs_readpage_nounlock(struct file *file, struct page *page)
{
	int ret = 0;
	hrfs_bindex_t bindex = 0;
	void *page_data = NULL;
	HENTRY();

	ret = hrfs_i_choose_bindex(page->mapping->host, HRFS_DATA_VALID, &bindex);
	if (ret) {
		HERROR("choose bindex failed, ret = %d\n", ret);
		goto out;
	}

	page_data = kmap(page);
	ret = hrfs_readpage_branch(file, page_data, bindex, page->index);
	kunmap(page);

	if (ret == 0) {
		SetPageUptodate(page);
	} else {
		ClearPageUptodate(page);
	}

out:
	HRETURN(ret);
}

int hrfs_readpage(struct file *file, struct page *page)
{
	int ret = 0;
	HENTRY();

	HASSERT(PageLocked(page));
	HASSERT(!PageUptodate(page));
	HASSERT(atomic_read(&file->f_dentry->d_inode->i_count) > 0);

	ret = hrfs_readpage_nounlock(file, page);
	unlock_page(page);

	HRETURN(ret);
}
EXPORT_SYMBOL(hrfs_readpage);

int hrfs_prepare_write(struct file *file, struct page *page, unsigned from, unsigned to)
{
	int ret = 0;
	HENTRY();
	HBUG(); /* Never needed */

	HASSERT(PageLocked(page));
	kmap(page);

	/* fast path for whole page writes */
	if (from == 0 && to == PAGE_CACHE_SIZE)
		goto out;

	/* read the page to "revalidate" our data */
	/* call the helper function which doesn't unlock the page */
	if (!PageUptodate(page)) {
		ret = hrfs_readpage_nounlock(file, page);
	}

out:
	HRETURN(ret);
}
EXPORT_SYMBOL(hrfs_prepare_write);

int hrfs_commit_write(struct file *file, struct page *page, unsigned from, unsigned to)
{
	int ret = 0;
	HENTRY();
	HBUG(); /* Never needed */

	/*
	 * We should probably use 1 << inode->i_blkbits instead of PAGE_SIZE here.
	 * However, stackable f/s copy i_blkbits from f/s below and it can be
	 * bigger than PAGE_SIZE (e.g., 32K for NFS).  PAGE_SIZE is hardcoded in
	 * the alloc_page_buffers and makes create_empty_buffers
	 * dereference a NULL pointer.  (Is it a kernel bug?)  Therefore, we have
	 * to hardcode PAGE_SIZE either here or during the i_blkbits init.
	 */
	if (!page_has_buffers(page))
			create_empty_buffers(page, PAGE_SIZE, 0);

	ret = generic_commit_write(file, page, from, to);
	kunmap(page);

	HRETURN(ret);
}
EXPORT_SYMBOL(hrfs_commit_write);

ssize_t hrfs_direct_IO(int rw, struct kiocb *kiocb,
						const struct iovec *iov, loff_t file_offset,
						unsigned long nr_segs)
{
	ssize_t ret = 0;
	struct iovec *iov_new = NULL;
	struct iovec *iov_tmp = NULL;
	size_t length = sizeof(*iov) * nr_segs;
	hrfs_bindex_t bindex = 0;
	struct file *hidden_file = NULL;
	struct file *file = kiocb->ki_filp;
	struct kiocb new_kiocb;
	HENTRY();
	
	HRFS_ALLOC(iov_new, length);
	if (!iov_new) {
		ret = -ENOMEM;
		goto out;
	}
	
	HRFS_ALLOC(iov_tmp, length);
	if (!iov_tmp) {
		ret = -ENOMEM;
		goto out_new_alloced;
	}	
	memcpy((char *)iov_new, (char *)iov, length); 
	
	if (hrfs_f2info(file) != NULL) { 
		for (bindex = 0; bindex < hrfs_f2bnum(file); bindex++) {
			hidden_file = hrfs_f2branch(file, bindex);
			if (!hidden_file) {
				HDEBUG("direct_IO of barnch[%d] file [%*s] is not supported\n",
				       bindex, file->f_dentry->d_name.len, file->f_dentry->d_name.name);
				continue;
			}
			HASSERT(hidden_file->f_mapping);
			HASSERT(hidden_file->f_mapping->a_ops);	
			HASSERT(hidden_file->f_mapping->a_ops->direct_IO);	

			memcpy((char *)iov_tmp, (char *)iov_new, length); 
			new_kiocb.ki_filp = hidden_file;
			ret = hidden_file->f_mapping->a_ops->direct_IO(rw, &new_kiocb, iov_tmp, file_offset, nr_segs);

			if (rw == READ) {
				if (ret > 0) {
					break;
				}
			}
		}
	}

//tmp_alloced_err:
	HRFS_FREE(iov_tmp, length);
out_new_alloced:
	HRFS_FREE(iov_new, length);
out:
	HRETURN(ret);
}
EXPORT_SYMBOL(hrfs_direct_IO);

/**
 * Page fault handler.
 *
 * \param vma - is virtiual area struct related to page fault
 * \param address - address when hit fault
 * \param type - of fault
 *
 * \return allocated and filled page for address
 * \retval NOPAGE_SIGBUS if page not exist on this address
 * \retval NOPAGE_OOM not have memory for allocate new page
 */
struct page *hrfs_nopage(struct vm_area_struct *vma, unsigned long address,
                         int *type)
{
	struct page *page = NULL;
	HENTRY();

	page = filemap_nopage(vma, address, type);
	if (page == NOPAGE_SIGBUS || page == NOPAGE_OOM) {
		HERROR("got addr %lu type %lx - SIGBUS\n", address, (long)type);
	}
	HRETURN(page);
}
EXPORT_SYMBOL(hrfs_nopage);

struct address_space_operations hrfs_aops =
{
	direct_IO:      hrfs_direct_IO,
	writepage:      hrfs_writepage,
	readpage:       hrfs_readpage,
	prepare_write:  hrfs_prepare_write,  /* Never needed */
	commit_write:   hrfs_commit_write    /* Never needed */
};


struct vm_operations_struct hrfs_file_vm_ops = {
	nopage:         hrfs_nopage,
	populate:       filemap_populate
};
