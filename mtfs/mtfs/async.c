#include <linux/module.h>
#include <memory.h>
#include <debug.h>
#include <spinlock.h>
#include <mtfs_interval_tree.h>
#include <mtfs_async.h>
#include <mtfs_list.h>
#include <mtfs_io.h>
#include <mtfs_file.h>
#include <mtfs_inode.h>
#include "file_internal.h"
#include "io_internal.h"
#include "main_internal.h"

void masync_bucket_init(struct masync_bucket *bucket)
{
	MENTRY();

	mtfs_spin_lock_init(&bucket->mab_lock);
	bucket->mab_root = NULL;
	bucket->mab_size = 0;

	_MRETURN();
}

static enum mtfs_interval_iter masync_overlap_cb(struct mtfs_interval_node *node,
                                                 void *args)
{
	struct list_head *extent_list = NULL;
	struct mtfs_interval *extent_node = NULL;

	extent_node = mtfs_node2interval(node);
	extent_list = (struct list_head *)args;
	mtfs_list_add(&extent_node->mi_linkage, extent_list);

	return MTFS_INTERVAL_ITER_CONT;
}

int masync_bucket_add(struct masync_bucket *bucket,
                       struct mtfs_interval_node_extent *extent)
{
	MTFS_LIST_HEAD(extent_list);
	struct mtfs_interval *tmp_extent = NULL;
	struct mtfs_interval *node = NULL;
	struct mtfs_interval *head = NULL;
	int ret = 0;
	__u64 min_start = extent->start;
	__u64 max_end = extent->end;
	MENTRY();

	MTFS_SLAB_ALLOC_PTR(node, mtfs_interval_cache);
	if (node == NULL) {
		MERROR("failed to create interval, not enough memory\n");
		ret = -ENOMEM;
		goto out;
	}

	MERROR("adding [%lu, %lu]\n", extent->start, extent->end);

	mtfs_spin_lock(&bucket->mab_lock);
	{
		mtfs_interval_search(bucket->mab_root,
		                     extent,
		                     masync_overlap_cb,
		                     &extent_list);

		mtfs_list_for_each_entry_safe(tmp_extent, head, &extent_list, mi_linkage) {
			MERROR("[%lu, %lu] overlaped with [%lu, %lu]\n",
			       tmp_extent->mi_node.in_extent.start,
			       tmp_extent->mi_node.in_extent.end,
			       extent->start, extent->end);
			if (tmp_extent->mi_node.in_extent.start < min_start) {
				min_start = tmp_extent->mi_node.in_extent.start;
			}
			if (tmp_extent->mi_node.in_extent.end > max_end) {
				max_end = tmp_extent->mi_node.in_extent.end;
			}
			mtfs_interval_erase(&tmp_extent->mi_node, &bucket->mab_root);
			MTFS_SLAB_FREE_PTR(tmp_extent, mtfs_interval_cache);
		}
		mtfs_interval_set(&node->mi_node, min_start, max_end);
		mtfs_interval_insert(&node->mi_node, &bucket->mab_root);
		MERROR("added [%lu, %lu]\n", min_start, max_end);
	}
	mtfs_spin_unlock(&bucket->mab_lock);

out:
	MRETURN(ret);
}

int masync_bucket_cleanup(struct masync_bucket *bucket)
{
	MTFS_LIST_HEAD(extent_list);
	struct mtfs_interval *node = NULL;
	int ret = 0;
	MENTRY();

	mtfs_spin_lock(&bucket->mab_lock);
	{
		while(bucket->mab_root) {
			node = mtfs_node2interval(bucket->mab_root);
			mtfs_interval_erase(bucket->mab_root, &bucket->mab_root);
			MTFS_SLAB_FREE_PTR(node, mtfs_interval_cache);
		}
	}
	mtfs_spin_unlock(&bucket->mab_lock);

	MRETURN(ret);
}

static void masync_io_iter_start_rw(struct mtfs_io *io)
{
	struct mtfs_io_rw *io_rw = &io->u.mi_rw;
	MENTRY();

	if (io->mi_bindex == 0) {
		mtfs_io_iter_start_rw_nonoplist(io);
	} else {
		struct mtfs_interval_node_extent extent = {
			.start = *(io_rw->ppos) - io->mi_result.size,
			.end = *(io_rw->ppos) - 1,
		};
		MASSERT(io->mi_type == MIT_WRITEV);
		MASSERT(io->mi_successful);
		MASSERT(extent.start >= 0);
		MASSERT(extent.end >= extent.start);
		masync_bucket_add(mtfs_i2bucket(io_rw->file->f_dentry->d_inode), &extent);
	}

	_MRETURN();
}

static void masync_io_iter_fini_rw(struct mtfs_io *io, int init_ret)
{
	MENTRY();

	if (unlikely(init_ret)) {
		io->mi_break = 1;
		MBUG();
		goto out;
	}

	if ((io->mi_bindex == io->mi_bnum - 1) ||
	    (io->mi_type == MIT_READV) ||
	    (io->mi_type == MIT_WRITEV && !io->mi_successful)) {
	    	io->mi_break = 1;
	} else {
		io->mi_bindex++;
	}
out:
	_MRETURN();
}

const struct mtfs_io_operations masync_io_ops[] = {
	[MIT_READV] = {
		.mio_init       = NULL,
		.mio_fini       = NULL,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = masync_io_iter_start_rw,
		.mio_iter_end   = NULL,
		.mio_iter_fini  = masync_io_iter_fini_rw,
	},
	[MIT_WRITEV] = {
		.mio_init       = NULL,
		.mio_fini       = NULL,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = masync_io_iter_start_rw,
		.mio_iter_end   = NULL,
		.mio_iter_fini  = masync_io_iter_fini_rw,
	},
};

static int masync_io_init_rw(struct mtfs_io *io, int is_write,
                             struct file *file, const struct iovec *iov,
                             unsigned long nr_segs, loff_t *ppos, size_t rw_size)
{
	int ret = 0;
	mtfs_io_type_t type;
	MENTRY();

	mtfs_io_init_rw_common(io, is_write, file, iov, nr_segs, ppos, rw_size);
	type = is_write ? MIT_WRITEV : MIT_READV;
	io->mi_ops = &masync_io_ops[type];

	MRETURN(ret);
}

static ssize_t masync_file_rw(int is_write, struct file *file, const struct iovec *iov,
                              unsigned long nr_segs, loff_t *ppos)
{
	ssize_t size = 0;
	int ret = 0;
	struct mtfs_io *io = NULL;
	size_t rw_size = 0;
	MENTRY();

	rw_size = get_iov_count(iov, &nr_segs);
	if (rw_size <= 0) {
		ret = rw_size;
		goto out;
	}

	MTFS_SLAB_ALLOC_PTR(io, mtfs_io_cache);
	if (io == NULL) {
		MERROR("not enough memory\n");
		size = -ENOMEM;
		goto out;
	}

	ret = masync_io_init_rw(io, is_write, file, iov, nr_segs, ppos, rw_size);
	if (ret) {
		size = ret;
		goto out_free_io;
	}

	ret = mtfs_io_loop(io);
	if (ret) {
		MERROR("failed to loop on io\n");
		size = ret;
	} else {
		size = io->mi_result.size;
	}

out_free_io:
	MTFS_SLAB_FREE_PTR(io, mtfs_io_cache);
out:
	MRETURN(size);
}

#ifdef HAVE_FILE_READV
ssize_t masync_file_readv(struct file *file, const struct iovec *iov,
                          unsigned long nr_segs, loff_t *ppos)
{
	ssize_t size = 0;
	MENTRY();

	size = masync_file_rw(READ, file, iov, nr_segs, ppos);

	MRETURN(size);
}
EXPORT_SYMBOL(masync_file_readv);

ssize_t masync_file_read(struct file *file, char __user *buf, size_t len,
                         loff_t *ppos)
{
	struct iovec local_iov = { .iov_base = (void __user *)buf,
	                           .iov_len = len };
	return masync_file_readv(file, &local_iov, 1, ppos);
}
EXPORT_SYMBOL(masync_file_read);

ssize_t masync_file_writev(struct file *file, const struct iovec *iov,
                        unsigned long nr_segs, loff_t *ppos)
{
	ssize_t size = 0;
	MENTRY();

	size = masync_file_rw(WRITE, file, iov, nr_segs, ppos);

	MRETURN(size);
}
EXPORT_SYMBOL(masync_file_writev);
#else /* !HAVE_FILE_READV */
ssize_t masync_file_aio_read(struct kiocb *iocb, const struct iovec *iov,
                             unsigned long nr_segs, loff_t pos)
{
	ssize_t size = 0;
	struct file *file = iocb->ki_filp;
	MENTRY();

	size = masync_file_rw(READ, file, iov, nr_segs, &pos);
	if (size > 0) {
		iocb->ki_pos = pos + size;
	}

	MRETURN(size);
}
EXPORT_SYMBOL(masync_file_aio_read);

ssize_t masync_file_aio_write(struct kiocb *iocb, const struct iovec *iov,
                              unsigned long nr_segs, loff_t pos)
{
	ssize_t size = 0;
	struct file *file = iocb->ki_filp;
	MENTRY();

	size = masync_file_rw(WRITE, file, iov, nr_segs, &pos);
	if (size > 0) {
		iocb->ki_pos = pos + size;
	}

	MRETURN(size);
}
EXPORT_SYMBOL(masync_file_aio_write);

#endif /* !HAVE_FILE_READV */
