/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <linux/sched.h>
#include <linux/mount.h>
#include <mtfs_interval_tree.h>
#include <mtfs_async.h>
#include <mtfs_dentry.h>
#include <mtfs_inode.h>
#include <mtfs_super.h>
#include "async_bucket_internal.h"
#include "async_extent_internal.h"
#include "async_chunk_internal.h"

/* Called holding mab_lock */
int masync_sync_file(struct masync_bucket *bucket,
                     struct mtfs_interval_node_extent *extent,
                     char *buf,
                     int buf_size)
{
	int ret = 0;
	struct file *src_file = NULL;
	struct file *dest_file = NULL;
	struct inode *inode = mtfs_bucket2inode(bucket);
	struct super_block *sb = inode->i_sb;
	struct msubject_async_info *info = NULL;
	loff_t pos = 0;
	loff_t tmp_pos = 0;
	size_t len = 0;
	size_t result = 0;
	MENTRY();

	info = (struct msubject_async_info *)mtfs_s2subinfo(sb);

	MASSERT(bucket->mab_fvalid);
	src_file = bucket->mab_finfo.barray[0].bfile;
	dest_file = bucket->mab_finfo.barray[1].bfile;
	MASSERT(src_file);
	MASSERT(dest_file);

	pos = extent->start;
	while (pos <= extent->end) {
		len = extent->end - pos + 1;
		if (len > buf_size) {
			len = buf_size;
		}

		tmp_pos = pos;
		result = _do_read_write(READ, src_file, buf, len, &tmp_pos);
		if (result != len) {
			/* TODO: MERROR */
			MERROR("failed to read extent [%lu, %lu] of file, "
			       "expected %ld, got %ld\n",
			       extent->start, extent->end,
			       len, result);
			if (result == 0) {
				MBUG();
				break;
			}

			if (result < 0) {
				ret = result;
				break;
			}

			len = result;
		}

		tmp_pos = pos;
		result = _do_read_write(WRITE, dest_file, buf, len, &tmp_pos);
		if (result != len) {
			MERROR("failed to write extent [%lu, %lu] of file, "
			       "expected %ld, got %ld\n",
			       extent->start, extent->end,
			       len, result);
			if (result >= 0) {
				ret = -EIO;
				break;
			}
		}

		pos += len;
	}

	MRETURN(ret);
}

void masync_bucket_init(struct msubject_async_info *info,
                        struct masync_bucket *bucket)
{
	MENTRY();

	bucket->mab_info = info;
	bucket->mab_root = NULL;
	atomic_set(&bucket->mab_number, 0);
	bucket->mab_fvalid = 0;
	init_MUTEX(&bucket->mab_lock);
	MTFS_INIT_LIST_HEAD(&bucket->mab_linkage);
	masync_bucket_add_to_list(bucket);
	init_MUTEX(&bucket->mab_chunk_lock);
	MTFS_INIT_LIST_HEAD(&bucket->mab_chunks);
	_MRETURN();
}

enum mtfs_interval_iter masync_overlap_cb(struct mtfs_interval_node *node,
                                          void *args)
{
	mtfs_list_t *extent_list = NULL;
	struct mtfs_interval *extent_node = NULL;

	extent_node = mtfs_node2interval(node);
	extent_list = (mtfs_list_t *)args;
	mtfs_list_add(&extent_node->mi_linkage, extent_list);

	return MTFS_INTERVAL_ITER_CONT;
}

/* Called when holding mab_lock */
static int masycn_bucket_fget(struct masync_bucket *bucket, struct file *file)
{
	int ret = 0;
	mtfs_bindex_t bindex = 0;
	struct dentry *dentry = file->f_dentry;
	struct inode *inode = dentry->d_inode;
	struct vfsmount *hidden_mnt = NULL;
	struct dentry *hidden_dentry = NULL;
	struct file *hidden_file = NULL;
	mtfs_bindex_t bnum = mtfs_f2bnum(file);
	int hidden_flags = O_RDWR | O_LARGEFILE;
	MENTRY();

	MASSERT(!bucket->mab_fvalid);

	for (bindex = 0; bindex < bnum; bindex++) {
		hidden_dentry = mtfs_d2branch(dentry, bindex);
		hidden_mnt = mtfs_s2mntbranch(inode->i_sb, bindex);
		dget(hidden_dentry);
		mntget(hidden_mnt);
		hidden_file = mtfs_dentry_open(hidden_dentry,
		                               hidden_mnt,
		                               hidden_flags,
		                               current_cred());
		if (IS_ERR(hidden_file)) {
			MERROR("open branch[%d] of file [%.*s], flags = 0x%x, ret = %ld\n", 
			       bindex, hidden_dentry->d_name.len, hidden_dentry->d_name.name,
			       hidden_flags, PTR_ERR(hidden_file));
			ret = PTR_ERR(hidden_file);
			MBUG();
		} else {
			bucket->mab_finfo.barray[bindex].bfile = hidden_file;
		}
	}
	bucket->mab_finfo.bnum = bnum;
	bucket->mab_fvalid = 1;

	MRETURN(ret);
}

/* Called when holding mab_lock */
static int masycn_bucket_fput(struct masync_bucket *bucket)
{
	int ret = 0;
	mtfs_bindex_t bindex = 0;
	mtfs_bindex_t bnum = bucket->mab_finfo.bnum;
	struct file *hidden_file = NULL;
	MENTRY();

	MASSERT(bucket->mab_fvalid);
	for (bindex = 0; bindex < bnum; bindex++) {
		hidden_file = bucket->mab_finfo.barray[bindex].bfile;
		MASSERT(hidden_file);
		fput(hidden_file);
	}
	bucket->mab_fvalid = 0;

	MRETURN(ret);
}

/*
 * Start adding bucket.
 * Any operation that may fail should be here.
 */
int masync_bucket_add_start(struct inode *inode,
                            struct masync_extent **async_extent)
{
	int ret = 0;
	struct masync_bucket *bucket = mtfs_i2bucket(inode);
	struct masync_extent *extent;
	MENTRY();

	extent = masync_extent_init(bucket);
	if (extent == NULL) {
		MERROR("failed to create interval, not enough memory\n");
		ret = -ENOMEM;
		goto out;
	}

	ret = masync_chunk_add_interval(bucket,
	                                &extent->mae_chunks,
	                                0,
	                                MASYNC_CHUNK_SIZE - 1);
	if (ret) {
		extent->mae_bucket = NULL;
		masync_extent_fini(extent);
	}
	*async_extent = extent;
out:
	MRETURN(ret);
}

/*
 * Never fail.
 */
void masync_bucket_add_end(struct file *file,
                           struct mtfs_interval_node_extent *interval,
                           struct masync_extent *async_extent)
{
	MTFS_LIST_HEAD(extent_list);
	struct mtfs_interval_node_extent tmp_interval;
	struct mtfs_interval *tmp_extent = NULL;
	struct mtfs_interval *node = NULL;
	struct mtfs_interval *head = NULL;
	struct masync_extent *tmp_async_extent = NULL;
	__u64 min_start = interval->start;
	__u64 max_end = interval->end;
	struct inode *inode = file->f_dentry->d_inode;
	struct masync_bucket *bucket = mtfs_i2bucket(inode);
	struct mtfs_interval_node *found = NULL;
	MENTRY();

	MDEBUG("adding [%lu, %lu]\n", interval->start, interval->end);

	/* Extent tree increase reference */
	masync_extent_get(async_extent);

	node = &async_extent->mae_interval;

	if (interval->start != 0) {
		tmp_interval.start = interval->start - 1;
	} else {
		tmp_interval.start = 0;
	}

	if (interval->end != MTFS_INTERVAL_EOF) {
		tmp_interval.end = interval->end + 1;
	} else {
		tmp_interval.end = MTFS_INTERVAL_EOF;
	}

	down(&bucket->mab_lock);
	mtfs_interval_search(bucket->mab_root,
	                     &tmp_interval,
	                     masync_overlap_cb,
	                     &extent_list);

	mtfs_list_for_each_entry(tmp_extent, &extent_list, mi_linkage) {
		if (tmp_extent->mi_node.in_extent.start < min_start) {
			min_start = tmp_extent->mi_node.in_extent.start;
		}
		if (tmp_extent->mi_node.in_extent.end > max_end) {
			max_end = tmp_extent->mi_node.in_extent.end;
		}

		atomic_dec(&bucket->mab_number);
		mtfs_interval_erase(&tmp_extent->mi_node, &bucket->mab_root);

		tmp_async_extent = masync_interval2extent(tmp_extent);
		masync_chunk_merge_extent(tmp_async_extent, async_extent);

		/* Export that this extent is not used since now */
		mtfs_spin_lock(&tmp_async_extent->mae_lock);
		tmp_async_extent->mae_bucket = NULL;
		mtfs_spin_unlock(&tmp_async_extent->mae_lock);
	}
	atomic_inc(&bucket->mab_number);
	mtfs_interval_set(&node->mi_node, min_start, max_end);
	found = mtfs_interval_insert(&node->mi_node, &bucket->mab_root);
	MASSERT(!found);

	if (!bucket->mab_fvalid) {
		masycn_bucket_fget(bucket, file);
	}
	/*
	 * Should be protected by mab_lock,
	 * since masync_bucket_cleanup() may miss it and cause memory leak.
	 */
	masync_extent_add_to_lru(async_extent);
	up(&bucket->mab_lock);

	/* Can be moved to background */
	mtfs_list_for_each_entry_safe(tmp_extent, head, &extent_list, mi_linkage) {
		tmp_async_extent = masync_interval2extent(tmp_extent);

		masync_extent_remove_from_lru(tmp_async_extent);

		/* Extent tree release reference */
		masync_extent_put(tmp_async_extent);
	}

	_MRETURN();
}

void masync_bucket_add_abort(struct file *file,
                             struct mtfs_interval_node_extent *interval,
                             struct masync_extent *async_extent)
{
	async_extent->mae_bucket = NULL;
	masync_extent_fini(async_extent);
}

/* Return extent_number canceled */
int masync_bucket_cleanup(struct masync_bucket *bucket)
{
	struct mtfs_interval *node = NULL;
	int ret = 0;
	int extent_number = 0;
	int buf_size = MASYNC_BULK_SIZE;
	char *buf = NULL;
	struct masync_extent *async_extent = NULL;
	MENTRY();

	MTFS_ALLOC(buf, buf_size);
	if (buf == NULL) {
		MERROR("not enough memory\n");
	}

	/* Cleanup extents */
	down(&bucket->mab_lock);
	extent_number = atomic_read(&bucket->mab_number);
	while (bucket->mab_root) {
		node = mtfs_node2interval(bucket->mab_root);
		if (buf != NULL) {
			if (bucket->mab_fvalid) {
				ret = masync_sync_file(bucket,
				                       &node->mi_node.in_extent,
				                       buf, buf_size);
				if (ret) {
					MERROR("failed sync file between branches\n");
					mtfs_inode_size_dump(mtfs_bucket2inode(bucket));
				}
			}
		}
		atomic_dec(&bucket->mab_number);
		mtfs_interval_erase(bucket->mab_root, &bucket->mab_root);

		async_extent = masync_interval2extent(node);

		/*
		 * If in LRU list, release it.
		 * If in cancel list, leave it to cancel thread.
		 * TODO: move it out of lock
		 */
		masync_extent_remove_from_lru(async_extent);

		/* Export that this extent is not used since now */
		mtfs_spin_lock(&async_extent->mae_lock);
		async_extent->mae_bucket = NULL;
		mtfs_spin_unlock(&async_extent->mae_lock);

		/* Extent tree release reference */
		masync_extent_put(async_extent);
	}
	if (bucket->mab_fvalid) {
		masycn_bucket_fput(bucket);
	}
	up(&bucket->mab_lock);

	/* Cleanup chunks */
	masync_chunk_cleanup(bucket);

	masync_bucket_remove_from_list(bucket);

	MASSERT(bucket->mab_root == NULL);
	MASSERT(atomic_read(&bucket->mab_number) == 0);
	MASSERT(!bucket->mab_fvalid);
	MASSERT(mtfs_list_empty(&bucket->mab_chunks));

	if (buf) {
		MTFS_FREE(buf, buf_size);
	}

	MRETURN(extent_number);
}

/* Called holding bucket->mab_lock */
static void _masync_bucket_remove(struct masync_bucket *bucket,
                                  struct masync_extent *async_extent)
{
	struct mtfs_interval *extent = &async_extent->mae_interval;

	atomic_dec(&bucket->mab_number);
	mtfs_interval_erase(&extent->mi_node, &bucket->mab_root);

	/* Export that this extent is not used since now */
	mtfs_spin_lock(&async_extent->mae_lock);
	async_extent->mae_bucket = NULL;
	mtfs_spin_unlock(&async_extent->mae_lock);

	masync_extent_remove_from_lru(async_extent);
	/* Extent tree release reference */
	masync_extent_put(async_extent);
}

static void _masync_bucket_add_end(struct masync_bucket *bucket,
                                   struct masync_extent *async_extent,
                                   struct mtfs_interval_node_extent *interval)
{
	struct mtfs_interval_node *found = NULL;
	MENTRY();

	masync_extent_get(async_extent);
	atomic_inc(&bucket->mab_number);
	mtfs_interval_set(&async_extent->mae_interval.mi_node,
	                  interval->start,
	                  interval->end);
	found = mtfs_interval_insert(&async_extent->mae_interval.mi_node,
	                             &bucket->mab_root);
	MASSERT(!found);
	masync_extent_add_to_lru(async_extent);

	_MRETURN();
}

/* TODO: share the same codes with masync_bucket_add_end() */
static int masync_bucket_extent_truncate(struct masync_bucket *bucket,
                                         struct masync_extent *extent,
                                         __u64 size)
{
	struct masync_extent *add_extent = NULL;
	struct mtfs_interval_node_extent add_interval;
	int ret = 0;
	MENTRY();

	MASSERT(extent->mae_interval.mi_node.in_extent.start < size);
	MASSERT(extent->mae_interval.mi_node.in_extent.end >= size);
	add_interval.start = extent->mae_interval.mi_node.in_extent.start;
	add_interval.end = size - 1;

	ret = masync_bucket_add_start(mtfs_bucket2inode(bucket), &add_extent);
	if (ret) {
		MERROR("failed to add extent to bucket, ret = %d\n",
		       ret);
		goto out;
	}
	
	_masync_bucket_remove(bucket, extent);
	_masync_bucket_add_end(bucket, add_extent, &add_interval);
out:
	MRETURN(ret);
}

int masync_bucket_truncate(struct masync_bucket *bucket, __u64 size)
{
	int ret = 0;
	MTFS_LIST_HEAD(extent_list);
	struct mtfs_interval_node_extent search_interval;
	struct mtfs_interval *list_interval = NULL;
	struct mtfs_interval *head_interval = NULL;
	struct masync_extent *extent = NULL;
	MENTRY();

	search_interval.start = size;
	search_interval.end = MTFS_INTERVAL_EOF;
	down(&bucket->mab_lock);
	mtfs_interval_search(bucket->mab_root, &search_interval,
	                     masync_overlap_cb, &extent_list);

	mtfs_list_for_each_entry_safe(list_interval, head_interval,
	                              &extent_list, mi_linkage) {
		extent = masync_interval2extent(list_interval);
		if (list_interval->mi_node.in_extent.start < size) {
			/* Remove old one and insert new one */
			ret = masync_bucket_extent_truncate(bucket,
			                                    extent,
			                                    size);
			if (ret) {
				break;
			}
		} else {
			/* Covered, remove the extent */
			_masync_bucket_remove(bucket, extent);
		}
	}
	up(&bucket->mab_lock);

	MRETURN(ret);
}

static enum mtfs_interval_iter masync_dump_overlap_cb(struct mtfs_interval_node *node,
                                                          void *args)
{
	MERROR("[%lu, %lu]\n",
	       node->in_extent.start,
	       node->in_extent.end);
	return MTFS_INTERVAL_ITER_CONT;
}

/* Called holding bucket->mab_lock */
void masync_extets_dump(struct masync_bucket *bucket)
{
	struct mtfs_interval_node_extent tmp_interval;

	MERROR("all:\n");
	tmp_interval.start = 0;
	tmp_interval.end = MTFS_INTERVAL_EOF;
	mtfs_interval_search(bucket->mab_root,
	                     &tmp_interval,
	                     masync_dump_overlap_cb,
	                     NULL);
	return;
}
