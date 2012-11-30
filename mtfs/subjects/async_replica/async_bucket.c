/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <mtfs_interval_tree.h>
#include <mtfs_async.h>
#include <mtfs_dentry.h>
#include <mtfs_inode.h>
#include <mtfs_super.h>
#include <linux/mount.h>
#include "async_bucket_internal.h"

static int masync_sync_file(struct masync_bucket *bucket,
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
			MDEBUG("failed to read file, expected %ld, got %ld\n",
			       len, result);
			if (result == 0) {
				/* Sync completed */
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
			MERROR("failed to write file, expected %ld, got %ld\n",
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
	mtfs_list_t *extent_list = NULL;
	struct mtfs_interval *extent_node = NULL;

	extent_node = mtfs_node2interval(node);
	extent_list = (mtfs_list_t *)args;
	mtfs_list_add(&extent_node->mi_linkage, extent_list);

	return MTFS_INTERVAL_ITER_CONT;
}

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

int masync_bucket_add(struct file *file,
                      struct mtfs_interval_node_extent *extent)
{
	MTFS_LIST_HEAD(extent_list);
	struct mtfs_interval *tmp_extent = NULL;
	struct mtfs_interval *node = NULL;
	struct mtfs_interval *head = NULL;
	int ret = 0;
	__u64 min_start = extent->start;
	__u64 max_end = extent->end;
	struct inode *inode = file->f_dentry->d_inode;
	struct masync_bucket *bucket = mtfs_i2bucket(inode);
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
		atomic_dec(&bucket->mab_info->msai_nr);
		atomic_dec(&masync_nr);
	}
	mtfs_interval_set(&node->mi_node, min_start, max_end);
	mtfs_interval_insert(&node->mi_node, &bucket->mab_root);
	atomic_inc(&bucket->mab_nr);
	atomic_inc(&bucket->mab_info->msai_nr);
	atomic_inc(&masync_nr);
	MDEBUG("added [%lu, %lu]\n", min_start, max_end);
	if (mtfs_list_empty(&bucket->mab_linkage)) {
		masycn_bucket_fget(bucket, file);
		masync_bucket_add_to_lru(bucket);
	} else {
		MASSERT(bucket->mab_fvalid);
		masync_bucket_touch_in_lru(bucket);
	}
	masync_bucket_unlock(bucket);
	//TODO: remove
	//masync_service_wakeup();
out:
	MRETURN(ret);
}

/* Return nr that canceled */
int masync_bucket_cleanup(struct masync_bucket *bucket)
{
	struct mtfs_interval *node = NULL;
	int ret = 0;
	int nr = 0;
	int buf_size = MASYNC_BULK_SIZE;
	char *buf = NULL;
	MENTRY();

	MERROR("cleanup %p\n", bucket);
	MTFS_ALLOC(buf, buf_size);
	if (buf == NULL) {
		MERROR("not enough memory\n");
	}

	masync_bucket_lock(bucket);
	nr = atomic_read(&bucket->mab_nr);
	while (bucket->mab_root) {
		node = mtfs_node2interval(bucket->mab_root);
		if (buf != NULL) {
			ret = masync_sync_file(bucket,
			                       &node->mi_node.in_extent,
			                       buf, buf_size);
			//MASSERT(!ret);
		}
		mtfs_interval_erase(bucket->mab_root, &bucket->mab_root);
		MTFS_SLAB_FREE_PTR(node, mtfs_interval_cache);
		atomic_dec(&bucket->mab_nr);
		atomic_dec(&bucket->mab_info->msai_nr);
		atomic_dec(&masync_nr);
	}

	if (!mtfs_list_empty(&bucket->mab_linkage)) {
		masycn_bucket_fput(bucket);
		masync_bucket_remove_from_lru(bucket);
	}
	masync_bucket_unlock(bucket);

	if (buf) {
		MTFS_FREE(buf, buf_size);
	}

	MRETURN(nr);
}

int masync_bucket_cancel(struct masync_bucket *bucket,
                         char *buf, int buf_len,
                         int nr_to_cacel)
{
	struct mtfs_interval *node = NULL;
	int nr = 0;
	int ret = 0;
	MENTRY();

	while (bucket->mab_root && nr < nr_to_cacel) {
		node = mtfs_node2interval(bucket->mab_root);
		ret = masync_sync_file(bucket,
		                       &node->mi_node.in_extent,
		                       buf, buf_len);
		//MASSERT(!ret);
		mtfs_interval_erase(bucket->mab_root, &bucket->mab_root);
		MTFS_SLAB_FREE_PTR(node, mtfs_interval_cache);
		atomic_dec(&bucket->mab_nr);
		atomic_dec(&bucket->mab_info->msai_nr);
		atomic_dec(&masync_nr);
		nr++;
	}
	if (bucket->mab_root == NULL) {
		if (!list_empty(&bucket->mab_linkage)) {
			masycn_bucket_fput(bucket);
			masync_bucket_remove_from_lru_nonlock(bucket);
		}
	}

	MRETURN(nr);
}
