#include <linux/module.h>
#include <linux/dcache.h>
#include <linux/mutex.h>
#include <memory.h>
#include <debug.h>
#include <spinlock.h>
#include <mtfs_interval_tree.h>
#include <mtfs_async.h>
#include <mtfs_list.h>
#include <mtfs_io.h>
#include <mtfs_file.h>
#include <mtfs_inode.h>
#include <mtfs_super.h>
#include <mtfs_subject.h>
#include <mtfs_device.h>
#include "file_internal.h"
#include "io_internal.h"
#include "main_internal.h"
#include "lowerfs_internal.h"

static inline void masync_bucket_lock_init(struct masync_bucket *bucket)
{
	init_MUTEX(&bucket->mab_lock);
}

/* Returns 0 if the mutex has
 * been acquired successfully
 * or 1 if it it cannot be acquired.
 */
static inline int masync_bucket_trylock(struct masync_bucket *bucket)
{
	return down_trylock(&bucket->mab_lock);
}

static inline void masync_bucket_lock(struct masync_bucket *bucket)
{
	down(&bucket->mab_lock);
}

static inline void masync_bucket_unlock(struct masync_bucket *bucket)
{
	up(&bucket->mab_lock);
}

static inline void masync_info_lock_init(struct msubject_async_info *info)
{
	init_rwsem(&info->msai_lock);
}

static inline void masync_info_read_lock(struct msubject_async_info *info)
{
	down_read(&info->msai_lock);
}

static inline void masync_info_read_unlock(struct msubject_async_info *info)
{
	up_read(&info->msai_lock);
}

static inline void masync_info_write_lock(struct msubject_async_info *info)
{
	down_write(&info->msai_lock);
}

static inline void masync_info_write_unlock(struct msubject_async_info *info)
{
	up_write(&info->msai_lock);
}

static void masync_bucket_remove_from_lru_nonlock(struct masync_bucket *bucket)
{
	struct msubject_async_info *info = NULL;
        MENTRY();

	info = bucket->mab_info;
	MASSERT(info);
	MASSERT(atomic_read(&bucket->mab_nr) == 0);
	MASSERT(!mtfs_list_empty(&bucket->mab_linkage));
	MASSERT(bucket->mab_root == NULL);
	mtfs_list_del_init(&bucket->mab_linkage);

	_MRETURN();
}

static void masync_bucket_remove_from_lru(struct masync_bucket *bucket)
{
	struct msubject_async_info *info = NULL;
        MENTRY();

	info = bucket->mab_info;
	MASSERT(info);
	MASSERT(atomic_read(&bucket->mab_nr) == 0);
	MASSERT(!mtfs_list_empty(&bucket->mab_linkage));
	MASSERT(bucket->mab_root == NULL);
	masync_info_write_lock(bucket->mab_info);
	masync_bucket_remove_from_lru_nonlock(bucket);
	masync_info_write_unlock(bucket->mab_info);

	_MRETURN();
}

static void masync_bucket_add_to_lru_nonlock(struct masync_bucket *bucket)
{
	struct msubject_async_info *info = NULL;
        MENTRY();

	info = bucket->mab_info;
	MASSERT(info);
	MASSERT(mtfs_list_empty(&bucket->mab_linkage));
	MASSERT(bucket->mab_root != NULL);
	MASSERT(atomic_read(&bucket->mab_nr) != 0);
	mtfs_list_add_tail(&bucket->mab_linkage, &info->msai_dirty_buckets);

	_MRETURN();
}

static void masync_bucket_add_to_lru(struct masync_bucket *bucket)
{
	struct msubject_async_info *info = NULL;
        MENTRY();

	info = bucket->mab_info;
	MASSERT(info);
	MASSERT(mtfs_list_empty(&bucket->mab_linkage));
	MASSERT(bucket->mab_root != NULL);
	MASSERT(atomic_read(&bucket->mab_nr) != 0);
	masync_info_write_lock(info);
	masync_bucket_add_to_lru_nonlock(bucket);
	masync_info_write_unlock(info);

	_MRETURN();
}

static void masync_bucket_degrade_in_lru_nonlock(struct masync_bucket *bucket)
{
	struct msubject_async_info *info = NULL;
        MENTRY();

	info = bucket->mab_info;
	MASSERT(info);
	MASSERT(!mtfs_list_empty(&bucket->mab_linkage));
	mtfs_list_move(&bucket->mab_linkage, &info->msai_dirty_buckets);

	_MRETURN();
}

static void masync_bucket_touch_in_lru_nonlock(struct masync_bucket *bucket)
{
	struct msubject_async_info *info = NULL;
        MENTRY();

	info = bucket->mab_info;
	MASSERT(info);
	MASSERT(!mtfs_list_empty(&bucket->mab_linkage));
	mtfs_list_move_tail(&bucket->mab_linkage, &info->msai_dirty_buckets);

	_MRETURN();
}

static void masync_bucket_touch_in_lru(struct masync_bucket *bucket)
{
	struct msubject_async_info *info = NULL;
        MENTRY();

	info = bucket->mab_info;
	MASSERT(info);
	MASSERT(!mtfs_list_empty(&bucket->mab_linkage));
	masync_info_write_lock(info);
	masync_bucket_touch_in_lru_nonlock(bucket);
	masync_info_write_unlock(info);

	_MRETURN();
}

void masync_bucket_init(struct msubject_async_info *info, struct masync_bucket *bucket)
{
	MENTRY();

	masync_bucket_lock_init(bucket);
	bucket->mab_root = NULL;
	bucket->mab_info = info;
	atomic_set(&bucket->mab_nr, 0);
	MTFS_INIT_LIST_HEAD(&bucket->mab_linkage);

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

static int masync_bucket_add(struct masync_bucket *bucket,
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

	MDEBUG("adding [%lu, %lu]\n", extent->start, extent->end);

	masync_bucket_lock(bucket);
	mtfs_interval_search(bucket->mab_root,
	                     extent,
	                     masync_overlap_cb,
	                     &extent_list);

	mtfs_list_for_each_entry_safe(tmp_extent, head, &extent_list, mi_linkage) {
		MDEBUG("[%lu, %lu] overlaped with [%lu, %lu]\n",
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
		atomic_dec(&bucket->mab_nr);
	}
	mtfs_interval_set(&node->mi_node, min_start, max_end);
	mtfs_interval_insert(&node->mi_node, &bucket->mab_root);
	atomic_inc(&bucket->mab_nr);
	MDEBUG("added [%lu, %lu]\n", min_start, max_end);
	if (mtfs_list_empty(&bucket->mab_linkage)) {
		masync_bucket_add_to_lru(bucket);
	} else {
		masync_bucket_touch_in_lru(bucket);
	}
	masync_bucket_unlock(bucket);

out:
	MRETURN(ret);
}

/* Return nr that canceled */
int masync_bucket_cleanup(struct masync_bucket *bucket)
{
	struct mtfs_interval *node = NULL;
	int ret = 0;
	MENTRY();

	masync_bucket_lock(bucket);
	ret = atomic_read(&bucket->mab_nr);
	while (bucket->mab_root) {
		node = mtfs_node2interval(bucket->mab_root);
		mtfs_interval_erase(bucket->mab_root, &bucket->mab_root);
		MTFS_SLAB_FREE_PTR(node, mtfs_interval_cache);
		atomic_dec(&bucket->mab_nr);
	}
	
	if (!mtfs_list_empty(&bucket->mab_linkage)) {
		masync_bucket_remove_from_lru(bucket);
	}
	masync_bucket_unlock(bucket);

	MRETURN(ret);
}

static int masync_calculate_all(struct msubject_async_info *info)
{
	int ret = 0;
	struct masync_bucket *bucket = NULL;
	MENTRY();

	masync_info_read_lock(info);
	mtfs_list_for_each_entry(bucket, &info->msai_dirty_buckets, mab_linkage) {
		ret += atomic_read(&bucket->mab_nr);
	}
	masync_info_read_unlock(info);
	MASSERT(ret >= 0);

	MRETURN(ret);
}

static int masync_bucket_cancel(struct masync_bucket *bucket, int nr_to_cacel)
{
	struct mtfs_interval *node = NULL;
	int ret = 0;
	MENTRY();

	while (bucket->mab_root && ret < nr_to_cacel) {
		node = mtfs_node2interval(bucket->mab_root);
		mtfs_interval_erase(bucket->mab_root, &bucket->mab_root);
		MTFS_SLAB_FREE_PTR(node, mtfs_interval_cache);
		atomic_dec(&bucket->mab_nr);
		ret++;
	}
	if (bucket->mab_root == NULL) {
		if (!list_empty(&bucket->mab_linkage)) {
			masync_bucket_remove_from_lru_nonlock(bucket);
		}
	}

	MRETURN(ret);
}

/* Return nr that canceled */
static int masync_cancel(struct msubject_async_info *info, int nr_to_cacel)
{
	int ret = 0;
	struct masync_bucket *bucket = NULL;
	struct masync_bucket *n = NULL;
	int canceled = 0;
	int degraded = 0;
	MENTRY();

	MASSERT(nr_to_cacel > 0);
	while (ret < nr_to_cacel) {
		degraded = 0;

		masync_info_write_lock(info);
		mtfs_list_for_each_entry_safe(bucket, n, &info->msai_dirty_buckets, mab_linkage) {
			if (masync_bucket_trylock(bucket) != 0) {
				/*
				 * Do not wait, degrade instead to free it next time.
				 * Not precisely fair, but that is OK.
				 */
				masync_bucket_degrade_in_lru_nonlock(bucket);
				degraded++;
				MDEBUG("a bucket is too busy to be freed\n");
				continue;
			}
	
			canceled = masync_bucket_cancel(bucket, nr_to_cacel - ret);
			masync_bucket_unlock(bucket);
			ret += canceled;
			if (ret >= nr_to_cacel) {
				break;
			}
		}
		masync_info_write_unlock(info);

		if (ret < nr_to_cacel && degraded == 0) {
			MERROR("only canceled %d/%d, but none is available\n",
			       ret, nr_to_cacel);
			break;
		}
	}

	MRETURN(ret);
}

static void masync_io_iter_start_rw(struct mtfs_io *io)
{
	struct mtfs_io_rw *io_rw = &io->u.mi_rw;
	struct mtfs_interval_node_extent extent;
	MENTRY();

	if (io->mi_bindex == 0) {
		if (io->mi_type == MIOT_WRITEV) {
			extent.start = *(io_rw->ppos);
			extent.end = extent.start + io_rw->rw_size - 1;
			masync_bucket_add(mtfs_i2bucket(io_rw->file->f_dentry->d_inode), &extent);
		}
		mtfs_io_iter_start_rw_nonoplist(io);
	} else {
		
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

	if (io->mi_bindex == io->mi_bnum - 1) {
	    	io->mi_break = 1;
	} else {
		io->mi_bindex++;
	}
out:
	_MRETURN();
}

const struct mtfs_io_operations masync_io_ops[] = {
	[MIOT_CREATE] = {
		.mio_init       = mtfs_io_init_oplist,
		.mio_fini       = mtfs_io_fini_oplist,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mtfs_io_iter_start_create,
		.mio_iter_end   = mtfs_io_iter_end_oplist,
		.mio_iter_fini  = mtfs_io_iter_fini_write_ops,
	},
	[MIOT_LINK] = {
		.mio_init       = mtfs_io_init_oplist,
		.mio_fini       = mtfs_io_fini_oplist,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mtfs_io_iter_start_link,
		.mio_iter_end   = mtfs_io_iter_end_oplist,
		.mio_iter_fini  = mtfs_io_iter_fini_write_ops,
	},
	[MIOT_UNLINK] = {
		.mio_init       = mtfs_io_init_oplist,
		.mio_fini       = mtfs_io_fini_oplist,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mtfs_io_iter_start_unlink,
		.mio_iter_end   = mtfs_io_iter_end_oplist,
		.mio_iter_fini  = mtfs_io_iter_fini_write_ops,
	},
	[MIOT_MKDIR] = {
		.mio_init       = mtfs_io_init_oplist,
		.mio_fini       = mtfs_io_fini_oplist,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mtfs_io_iter_start_mkdir,
		.mio_iter_end   = mtfs_io_iter_end_oplist,
		.mio_iter_fini  = mtfs_io_iter_fini_write_ops,
	},
	[MIOT_RMDIR] = {
		.mio_init       = mtfs_io_init_oplist,
		.mio_fini       = mtfs_io_fini_oplist,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mtfs_io_iter_start_rmdir,
		.mio_iter_end   = mtfs_io_iter_end_oplist,
		.mio_iter_fini  = mtfs_io_iter_fini_write_ops,
	},
	[MIOT_MKNOD] = {
		.mio_init       = mtfs_io_init_oplist,
		.mio_fini       = mtfs_io_fini_oplist,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mtfs_io_iter_start_mknod,
		.mio_iter_end   = mtfs_io_iter_end_oplist,
		.mio_iter_fini  = mtfs_io_iter_fini_write_ops,
	},
	[MIOT_RENAME] = {
		.mio_init       = mtfs_io_init_oplist,
		.mio_fini       = mtfs_io_fini_oplist_rename,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mtfs_io_iter_start_rename,
		.mio_iter_end   = mtfs_io_iter_end_oplist,
		.mio_iter_fini  = mtfs_io_iter_fini_write_ops,
	},
	[MIOT_SYMLINK] = {
		.mio_init       = mtfs_io_init_oplist,
		.mio_fini       = mtfs_io_fini_oplist,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mtfs_io_iter_start_symlink,
		.mio_iter_end   = mtfs_io_iter_end_oplist,
		.mio_iter_fini  = mtfs_io_iter_fini_write_ops,
	},
	[MIOT_READLINK] = {
		.mio_init       = mtfs_io_init_oplist,
		.mio_fini       = NULL,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mtfs_io_iter_start_readlink,
		.mio_iter_end   = NULL,
		.mio_iter_fini  = mtfs_io_iter_fini_read_ops,
	},
	[MIOT_PERMISSION] = {
		.mio_init       = mtfs_io_init_oplist,
		.mio_fini       = NULL,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mtfs_io_iter_start_permission,
		.mio_iter_end   = NULL,
		.mio_iter_fini  = mtfs_io_iter_fini_read_ops,
	},
	[MIOT_READV] = {
		.mio_init       = NULL,
		.mio_fini       = NULL,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = masync_io_iter_start_rw,
		.mio_iter_end   = NULL,
		.mio_iter_fini  = masync_io_iter_fini_rw,
	},
	[MIOT_WRITEV] = {
		.mio_init       = NULL,
		.mio_fini       = NULL,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = masync_io_iter_start_rw,
		.mio_iter_end   = NULL,
		.mio_iter_fini  = masync_io_iter_fini_rw,
	},
	[MIOT_GETATTR] = {
		.mio_init       = mtfs_io_init_oplist,
		.mio_fini       = NULL,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mtfs_io_iter_start_getattr,
		.mio_iter_end   = NULL,
		.mio_iter_fini  = mtfs_io_iter_fini_read_ops,
	},
	[MIOT_SETATTR] = {
		.mio_init       = mtfs_io_init_oplist,
		.mio_fini       = mtfs_io_fini_oplist,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mtfs_io_iter_start_setattr,
		.mio_iter_end   = mtfs_io_iter_end_oplist,
		.mio_iter_fini  = mtfs_io_iter_fini_write_ops,
	},
	[MIOT_GETXATTR] = {
		.mio_init       = mtfs_io_init_oplist,
		.mio_fini       = NULL,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mtfs_io_iter_start_getxattr,
		.mio_iter_end   = NULL,
		.mio_iter_fini  = mtfs_io_iter_fini_read_ops,
	},
	[MIOT_SETXATTR] = {
		.mio_init       = mtfs_io_init_oplist,
		.mio_fini       = mtfs_io_fini_oplist,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mtfs_io_iter_start_setxattr,
		.mio_iter_end   = mtfs_io_iter_end_oplist,
		.mio_iter_fini  = mtfs_io_iter_fini_write_ops,
	},
	[MIOT_REMOVEXATTR] = {
		.mio_init       = mtfs_io_init_oplist,
		.mio_fini       = mtfs_io_fini_oplist,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mtfs_io_iter_start_removexattr,
		.mio_iter_end   = mtfs_io_iter_end_oplist,
		.mio_iter_fini  = mtfs_io_iter_fini_write_ops,
	},
	[MIOT_LISTXATTR] = {
		.mio_init       = mtfs_io_init_oplist,
		.mio_fini       = NULL,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mtfs_io_iter_start_listxattr,
		.mio_iter_end   = NULL,
		.mio_iter_fini  = mtfs_io_iter_fini_read_ops,
	},
	[MIOT_READDIR] = {
		.mio_init       = mtfs_io_init_oplist,
		.mio_fini       = NULL,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mtfs_io_iter_start_readdir,
		.mio_iter_end   = NULL,
		.mio_iter_fini  = mtfs_io_iter_fini_read_ops,
	},
	[MIOT_SETATTR] = {
		.mio_init       = mtfs_io_init_oplist,
		.mio_fini       = mtfs_io_fini_oplist,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mtfs_io_iter_start_setattr,
		.mio_iter_end   = mtfs_io_iter_end_oplist,
		.mio_iter_fini  = mtfs_io_iter_fini_write_ops,
	},
	[MIOT_OPEN] = {
		.mio_init       = mtfs_io_init_oplist,
		.mio_fini       = mtfs_io_fini_oplist_noupdate,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mtfs_io_iter_start_open,
		.mio_iter_end   = mtfs_io_iter_end_oplist,
		.mio_iter_fini  = mtfs_io_iter_fini_write_ops,
	},
};
EXPORT_SYMBOL(masync_io_ops);

static int masync_io_init_rw(struct mtfs_io *io, int is_write,
                             struct file *file, const struct iovec *iov,
                             unsigned long nr_segs, loff_t *ppos, size_t rw_size)
{
	int ret = 0;
	mtfs_io_type_t type;
	MENTRY();

	mtfs_io_init_rw_common(io, is_write, file, iov, nr_segs, ppos, rw_size);
	type = is_write ? MIOT_WRITEV : MIOT_READV;
	io->mi_ops = &((*(mtfs_f2ops(file)->io_ops))[type]);

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
		size = io->mi_result.ssize;
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

/* Could not put it into struct msubject_async_info */
static struct shrinker *masync_shrinker;
static struct msubject_async_info *masync_info;

static int _masync_shrink(int nr_to_scan, unsigned int gfp_mask)
{
	int ret = 0;
	int canceled = 0;
	MENTRY();

	if (nr_to_scan == 0) {
		goto out_calc;
	} else if (!(gfp_mask & __GFP_FS)) {
		ret = -1;
		goto out;
	}

	canceled = masync_cancel(masync_info, nr_to_scan);
	if (canceled != nr_to_scan) {
		MERROR("trying to shrink %d, canceled %d\n",
		       nr_to_scan, canceled);
	}

out_calc:
	ret = (masync_calculate_all(masync_info) / 100) * sysctl_vfs_cache_pressure;
out:
	MRETURN(ret);
}

static int masync_shrink(SHRINKER_ARGS(sc, nr_to_scan, gfp_mask))
{
	return _masync_shrink(shrink_param(sc, nr_to_scan),
                              shrink_param(sc, gfp_mask));
}

static int masync_proc_read_dirty(char *page, char **start, off_t off, int count,
                          int *eof, void *data)
{
	int ret = 0;
	struct msubject_async_info *async_info = NULL;
	struct masync_bucket *bucket = NULL;
	MENTRY();

	*eof = 1;

	async_info = (struct msubject_async_info *)data;

	masync_info_read_lock(async_info);
	mtfs_list_for_each_entry(bucket, &async_info->msai_dirty_buckets, mab_linkage) {
		if (ret >= count - 1) {
			MERROR("not enough memory for proc read\n");
			break;
		}
		ret += snprintf(page + ret, count - ret, "Inode: %p, NR: %d\n",
		                mtfs_bucket2inode(bucket), atomic_read(&bucket->mab_nr));
	}
	masync_info_read_unlock(async_info);

	MRETURN(ret);
}

static struct mtfs_proc_vars masync_proc_vars[] = {
	{ "dirty", masync_proc_read_dirty, NULL, NULL },
	{ 0 }
};

#define MASYNC_PROC_NAME "async_replica"
static int masync_info_proc_init(struct msubject_async_info *async_info,
                          struct super_block *sb)
{
	int ret = 0;
	struct mtfs_device *device = mtfs_s2dev(sb);
	MENTRY();

	async_info->msai_proc_entry = mtfs_proc_register(MASYNC_PROC_NAME,
	                                           mtfs_dev2proc(device),
	                                           masync_proc_vars,
	                                           async_info);
	if (unlikely(async_info->msai_proc_entry == NULL)) {
		MERROR("failed to register proc for async replica\n");
		ret = -ENOMEM;
	}

	MRETURN(ret);
}

static int masync_info_proc_fini(struct msubject_async_info *info,
                          struct super_block *sb)
{
	int ret = 0;
	MENTRY();

	mtfs_proc_remove(&info->msai_proc_entry);

	MRETURN(ret);
}

int masync_super_init(struct super_block *sb)
{
	int ret = 0;
	struct msubject_async_info *info = NULL;
	MENTRY();

	MTFS_ALLOC_PTR(info);
	if (info == NULL) {
		ret = -ENOMEM;
		MERROR("not enough memory\n");
		goto out;
	}

	MTFS_INIT_LIST_HEAD(&info->msai_dirty_buckets);
	masync_info_lock_init(info);

	ret = masync_info_proc_init(info, sb);
	if (ret) {
		goto out_free_info;
	}

	masync_info = info;
        masync_shrinker = mtfs_set_shrinker(DEFAULT_SEEKS, masync_shrink);
	mtfs_s2subinfo(sb) = (void *)info;
	goto out;
out_free_info:
	MTFS_FREE_PTR(info);
out:
	MRETURN(ret);
}

int masync_super_fini(struct super_block *sb)
{
	int ret = 0;
	struct msubject_async_info *info = NULL;
	MENTRY();

	info = (struct msubject_async_info *)mtfs_s2subinfo(sb);
	MASSERT(mtfs_list_empty(&info->msai_dirty_buckets));

	mtfs_remove_shrinker(masync_shrinker);
	masync_info_proc_fini(info, sb);

	MTFS_FREE_PTR(info);

	MRETURN(ret);
}

int masync_inode_init(struct inode *inode)
{
	int ret = 0;
	mtfs_bindex_t bindex = 0;
	mtfs_bindex_t bnum = mtfs_i2bnum(inode);
	MENTRY();

	masync_bucket_init((struct msubject_async_info *)mtfs_s2subinfo(inode->i_sb),
	                   mtfs_i2bucket(inode));

	for (bindex = 0; bindex < bnum; bindex++) {
		if (mtfs_i2branch(inode, bindex)) {
			mlowerfs_bucket_init(mtfs_i2bbucket(inode, bindex));
		}
	}
	MRETURN(ret);
}

int masync_inode_fini(struct inode *inode)
{
	int ret = 0;
	MENTRY();

	masync_bucket_cleanup(mtfs_i2bucket(inode));
	MRETURN(ret);
}

struct mtfs_subject_operations masync_subject_ops = {
	mso_super_init:                 masync_super_init,
	mso_super_fini:                 masync_super_fini,
	mso_inode_init:                 masync_inode_init,
	mso_inode_fini:                 masync_inode_fini,
};
EXPORT_SYMBOL(masync_subject_ops);
