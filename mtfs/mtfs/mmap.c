/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include "mtfs_internal.h"
#include "mmap_internal.h"
#include <linux/uio.h>
#include <mtfs_oplist.h>

static int mtfs_writepage_branch(struct page *page, struct inode *inode, mtfs_bindex_t bindex,
                                 unsigned hidden_page_index, struct writeback_control *wbc)
{
	int ret = 0;
	struct inode *lower_inode = NULL;
	struct page *lower_page = NULL;
	char *kaddr = NULL;
	char *lower_kaddr = NULL;
	HENTRY();

	ret = mtfs_device_branch_errno(mtfs_i2dev(inode), bindex, BOPS_MASK_WRITE);
	if (ret) {
		HDEBUG("branch[%d] is abandoned\n", bindex);
		goto out; 
	}

	lower_inode = mtfs_i2branch(inode, bindex);
	if (lower_inode == NULL) {
		ret = -ENOENT;
		goto out;
	}

	/* This will lock_page */
	lower_page = grab_cache_page(lower_inode->i_mapping, hidden_page_index);
	if (!lower_page) {
		ret = -ENOMEM; /* Which errno*/
		goto out;
	}

	/*
	 * Writepage is not allowed when writeback is in progress.
	 * And PageWriteback flag will be set by writepage in some fs.
	 * So wait until it finishs.
	 */
	wait_on_page_writeback(lower_page);
	HASSERT(!PageWriteback(lower_page));

	/* Copy data to lower_page */
	kaddr = kmap(page);
	lower_kaddr = kmap(lower_page);
	memcpy(lower_kaddr, kaddr, PAGE_CACHE_SIZE);
	flush_dcache_page(lower_page);
	kunmap(lower_page);
	kunmap(page);

	set_page_dirty(lower_page);

	if (!clear_page_dirty_for_io(lower_page)) {
		HERROR("page(%p) is not dirty\n", lower_page);
		unlock_page(lower_page);
		page_cache_release(lower_page);
		HBUG();
		goto out;
	}

	/* This will unlock_page */
	ret = lower_inode->i_mapping->a_ops->writepage(lower_page, wbc);
	if (ret == AOP_WRITEPAGE_ACTIVATE) {
		HDEBUG("lowerfs choose to not start writepage\n");
		unlock_page(lower_page);
		ret = 0;
	}

	page_cache_release(lower_page); /* because read_cache_page increased refcnt */
out:
	HRETURN(ret);
}

int mtfs_writepage(struct page *page, struct writeback_control *wbc)
{
	int ret = 0;
	struct inode *inode = page->mapping->host;
	mtfs_bindex_t i = 0;
	mtfs_bindex_t bindex = 0;
	struct mtfs_operation_list *list = NULL;
	mtfs_operation_result_t result = {0};
	HENTRY();

	list = mtfs_oplist_build(inode);
	if (unlikely(list == NULL)) {
		HERROR("failed to build operation list\n");
		ret = -ENOMEM;
		goto out;
	}

	if (list->latest_bnum == 0) {
		HERROR("page has no valid branch, please check it\n");
		if (!(mtfs_i2dev(inode)->no_abort)) {
			ret = -EIO;
			goto out_free_oplist;
		}
	}

	for (i = 0; i < mtfs_i2bnum(inode); i++) {
		bindex = list->op_binfo[i].bindex;
		ret = mtfs_writepage_branch(page, inode, bindex, page->index, wbc);
		result.ret = ret;
		mtfs_oplist_setbranch(list, i, (ret == 0 ? 1 : 0), result);
		if (i == list->latest_bnum - 1) {
			mtfs_oplist_check(list);
			if (list->success_latest_bnum <= 0) {
				HDEBUG("operation failed for all latest branches\n");
				if (!(mtfs_i2dev(inode)->no_abort)) {
					ret = -EIO;
					goto out_free_oplist;
				}
			}
		}
	}

	mtfs_oplist_check(list);
	if (list->success_bnum <= 0) {
		result = mtfs_oplist_result(list);
		ret = result.size;
		goto out_free_oplist;
	}

	ret = mtfs_oplist_update(inode, list);
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
	mtfs_oplist_free(list);
out:
	unlock_page(page);
	HRETURN(ret);
}
EXPORT_SYMBOL(mtfs_writepage);

struct page *mtfs_read_cache_page(struct file *file, struct inode *hidden_inode, mtfs_bindex_t bindex, unsigned page_index)
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

static int mtfs_readpage_branch(struct file *file, char *page_data, mtfs_bindex_t bindex, unsigned hidden_page_index)
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

	HASSERT(mtfs_f2info(file));
	hidden_file = mtfs_f2branch(file, bindex);
	hidden_inode = mtfs_i2branch(inode, bindex);

	hidden_page = mtfs_read_cache_page(hidden_file, hidden_inode, bindex, hidden_page_index);
	if (IS_ERR(hidden_page)) {
		ret = PTR_ERR(hidden_page);
		goto out;
	}

	copy_page_data(page_data, hidden_page, hidden_inode, hidden_page_index);
	page_cache_release(hidden_page);

out:
	HRETURN(ret);
}

static int mtfs_readpage_nounlock(struct file *file, struct page *page)
{
	int ret = 0;
	mtfs_bindex_t bindex = 0;
	void *page_data = NULL;
	HENTRY();

	ret = mtfs_i_choose_bindex(page->mapping->host, MTFS_DATA_VALID, &bindex);
	if (ret) {
		HERROR("choose bindex failed, ret = %d\n", ret);
		goto out;
	}

	page_data = kmap(page);
	ret = mtfs_readpage_branch(file, page_data, bindex, page->index);
	kunmap(page);

	if (ret == 0) {
		SetPageUptodate(page);
	} else {
		ClearPageUptodate(page);
	}

out:
	HRETURN(ret);
}

int mtfs_readpage(struct file *file, struct page *page)
{
	int ret = 0;
	HENTRY();

	HASSERT(PageLocked(page));
	HASSERT(!PageUptodate(page));
	HASSERT(atomic_read(&file->f_dentry->d_inode->i_count) > 0);

	ret = mtfs_readpage_nounlock(file, page);
	unlock_page(page);

	HRETURN(ret);
}
EXPORT_SYMBOL(mtfs_readpage);

ssize_t mtfs_direct_IO(int rw, struct kiocb *kiocb,
						const struct iovec *iov, loff_t file_offset,
						unsigned long nr_segs)
{
	ssize_t ret = 0;
	struct iovec *iov_new = NULL;
	struct iovec *iov_tmp = NULL;
	size_t length = sizeof(*iov) * nr_segs;
	mtfs_bindex_t bindex = 0;
	struct file *hidden_file = NULL;
	struct file *file = kiocb->ki_filp;
	struct kiocb new_kiocb;
	HENTRY();
	
	MTFS_ALLOC(iov_new, length);
	if (!iov_new) {
		ret = -ENOMEM;
		goto out;
	}
	
	MTFS_ALLOC(iov_tmp, length);
	if (!iov_tmp) {
		ret = -ENOMEM;
		goto out_new_alloced;
	}	
	memcpy((char *)iov_new, (char *)iov, length); 
	
	if (mtfs_f2info(file) != NULL) { 
		for (bindex = 0; bindex < mtfs_f2bnum(file); bindex++) {
			hidden_file = mtfs_f2branch(file, bindex);
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
	MTFS_FREE(iov_tmp, length);
out_new_alloced:
	MTFS_FREE(iov_new, length);
out:
	HRETURN(ret);
}
EXPORT_SYMBOL(mtfs_direct_IO);

#ifdef HAVE_VM_OP_FAULT
int mtfs_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	int ret = 0;
	HENTRY();

	ret = filemap_fault(vma, vmf);
	HRETURN(ret);
}
EXPORT_SYMBOL(mtfs_fault);
#else /* ! HAVE_VM_OP_FAULT */
struct page *mtfs_nopage(struct vm_area_struct *vma, unsigned long address,
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
EXPORT_SYMBOL(mtfs_nopage);
#endif /* ! HAVE_VM_OP_FAULT */

struct address_space_operations mtfs_aops =
{
	direct_IO:      mtfs_direct_IO,
	writepage:      mtfs_writepage,
	readpage:       mtfs_readpage,
};

struct vm_operations_struct mtfs_file_vm_ops = {
#ifdef HAVE_VM_OP_FAULT
	fault:         mtfs_fault,
#else /* ! HAVE_VM_OP_FAULT */
	nopage:         mtfs_nopage,
	populate:       filemap_populate,
#endif /* ! HAVE_VM_OP_FAULT */
};
