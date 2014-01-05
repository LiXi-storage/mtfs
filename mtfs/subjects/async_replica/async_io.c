/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <linux/module.h>
#include <mtfs_io.h>
#include <mtfs_file.h>
#include <mtfs_dentry.h>
#include <mtfs_inode.h>
#include <mtfs_device.h>
#include <mtfs_interval_tree.h>
#include <mtfs_log.h>
#include <mtfs_context.h>
#include "async_internal.h"
#include "async_bucket_internal.h"

static int masync_bucket_fvalid(struct file *file)
{
	int ret = 1;
	struct dentry *dentry = file->f_dentry;
	mtfs_bindex_t bindex = 0;
	mtfs_bindex_t bnum = mtfs_f2bnum(file);
	struct dentry *hidden_dentry = NULL;
	MENTRY();

	for (bindex = 0; bindex < bnum; bindex++) {
		hidden_dentry = mtfs_d2branch(dentry, bindex);
		if (hidden_dentry == NULL) {
			ret = 0;
			break;
		}
	}

	MRETURN(ret);
}

static enum mtfs_interval_iter masync_checksum_overlap_cb(struct mtfs_interval_node *node,
                                                          void *args)
{
	return MTFS_INTERVAL_ITER_STOP;
}

static int masync_extent_cmp(void *priv, mtfs_list_t *a, mtfs_list_t *b)
{
	int ret = 0;
	struct mtfs_interval *extent_a = NULL;
	struct mtfs_interval *extent_b = NULL;
	MENTRY();

	extent_a = mtfs_list_entry(a, struct mtfs_interval, mi_linkage);
	extent_b = mtfs_list_entry(b, struct mtfs_interval, mi_linkage);

	if (extent_a->mi_node.in_extent.start >  extent_b->mi_node.in_extent.start) {
		MASSERT(extent_a->mi_node.in_extent.start > extent_b->mi_node.in_extent.end);
		ret = 1;
	} else {
		MASSERT(extent_a->mi_node.in_extent.end < extent_b->mi_node.in_extent.start);
		ret = -1;
	}

	MRETURN(ret);
}

/* Called holding bucket->mab_lock */
static void masync_dirty_extets_build(struct mtfs_io *io)
{
	struct mtfs_io_rw *io_rw = &io->u.mi_rw;
	struct mtfs_io_checksum *checksum = &io->subject.mi_checksum;
	struct file *file = io_rw->file;
	struct dentry *dentry = file->f_dentry;
	struct inode *inode = dentry->d_inode;
	struct masync_bucket *bucket = mtfs_i2bucket(inode);
	struct mtfs_interval_node_extent tmp_interval;
	loff_t pos = *(io_rw->ppos);
	MENTRY();

	MTFS_INIT_LIST_HEAD(&checksum->dirty_extents);
	/* 
	 * Dirty extents between [pos, pos + rw_size - 1].
	 */
	tmp_interval.start = pos;
	tmp_interval.end = pos + io_rw->rw_size - 1;
	mtfs_interval_search(bucket->mab_root,
	                     &tmp_interval,
	                     masync_overlap_cb,
	                     &checksum->dirty_extents);

	mtfs_list_sort(NULL, &checksum->dirty_extents, masync_extent_cmp);
	_MRETURN();
}

static void masync_dirty_extets_dump(mtfs_list_t *list)
{
	struct mtfs_interval *tmp_extent = NULL;
	MERROR("dirty:\n");
	mtfs_list_for_each_entry(tmp_extent, list, mi_linkage) {
		MERROR("[%lu, %lu]\n",
		       tmp_extent->mi_node.in_extent.start,
		       tmp_extent->mi_node.in_extent.end);
	}
}

static __u32 masync_checksum_compute(__u32 checksum,
                                     unsigned char const *p,
                                     size_t len,
                                     size_t offset,
                                     mtfs_list_t *head,
                                     struct mtfs_interval **extent,
                                     mchecksum_type_t type)
{
	__u32 checksum_value = checksum;
	struct mtfs_interval *tmp_extent = *extent;
	size_t tmp_offset = offset;
	size_t tmp_len = 0;
	size_t tmp_total = 0;                      /* Calculated length */
	unsigned char const *data = p; 
	MENTRY();

	MASSERT(len > 0);

	/* Skip dirty extents between [offset, offset + len - 1] */
	list_for_each_entry_from(tmp_extent, head, mi_linkage) {
		if (tmp_extent->mi_node.in_extent.start > tmp_offset) {
			tmp_len = tmp_extent->mi_node.in_extent.start -
			          tmp_offset;

			if (tmp_total + tmp_len > len) {
				tmp_len = len - tmp_total;
			}

			if (tmp_len == 0) {
				break;
			}

			MDEBUG("checksum_value = 0x%x, "
			       "len = %lu\n",
			       checksum_value,
			       tmp_len);
			checksum_value = mchecksum_compute(checksum_value,
			                                   data,
			                                   tmp_len,
			                                   type);
			MDEBUG("checksum_value = 0x%x\n", checksum_value);
			tmp_total += tmp_len;
			data += tmp_len;
			tmp_offset += tmp_len;
		 	if (tmp_total >= len) {
		 		MASSERT(tmp_total == len);
		 		break;
		 	}
		}

		MASSERT(tmp_extent->mi_node.in_extent.start <= tmp_offset);
		if (tmp_extent->mi_node.in_extent.end >= tmp_offset) {
			/* Skip */
			tmp_len = tmp_extent->mi_node.in_extent.end -
			          tmp_offset + 1;

			if (tmp_total + tmp_len >= len) {
				tmp_len = len - tmp_total;
			}

			tmp_total += tmp_len;
			data += tmp_len;
			tmp_offset += tmp_len;
		 	if (tmp_total >= len) {
		 		MASSERT(tmp_total == len);
		 		break;
		 	}
		}
	}

	if (tmp_total < len) {
		MASSERT(&tmp_extent->mi_linkage == head);
		/* All dirty extents run out */
		tmp_len = len - tmp_total;
		checksum_value = mchecksum_compute(checksum_value,
		                                   data,
		                                   tmp_len,
		                                   type);
		tmp_total += tmp_len;
		data += tmp_len;
		tmp_offset += tmp_len;
	}

	MASSERT(tmp_total == len);
	MASSERT(data == p + len);
	MASSERT(tmp_offset == offset + len);

	*extent = tmp_extent;
	MRETURN(checksum_value);
}

static __u32 masync_checksum_fill(__u32 checksum,
                                  size_t *len,
                                  size_t offset,
                                  size_t end,
                                  mtfs_list_t *head,
                                  struct mtfs_interval **extent,
                                  mchecksum_type_t type)
{
	__u32 checksum_value = checksum;
	struct mtfs_interval *tmp_extent = *extent;
	size_t tmp_offset = offset;
	size_t tmp_len = 0;
	size_t tmp_total = 0;
	char data[1] = {0};
	size_t i  = 0;
	MENTRY();

	MASSERT(len > 0);

	/* Skip dirty extents between [offset, offset + len - 1] */
	list_for_each_entry_from(tmp_extent, head, mi_linkage) {
		if (tmp_extent->mi_node.in_extent.start > tmp_offset) {
			MASSERT(tmp_extent->mi_node.in_extent.start <= end);
			tmp_len = tmp_extent->mi_node.in_extent.start -
			          tmp_offset;

			for (i = 0; i < tmp_len; i++) {
				checksum_value = mchecksum_compute(checksum_value,
				                                   data,
				                                   1,
				                                   type);
			}
			tmp_total += tmp_len;
			tmp_offset += tmp_len;
		}

		MASSERT(tmp_extent->mi_node.in_extent.start <= tmp_offset);
		if (tmp_extent->mi_node.in_extent.end >= tmp_offset) {
			/* Skip */
			if (tmp_extent->mi_node.in_extent.end > end) {
				/* It can be assert that this is the last extent */
				tmp_len = end - tmp_offset + 1;
			} else {
				tmp_len = tmp_extent->mi_node.in_extent.end -
				          tmp_offset + 1;
			}
			tmp_total += tmp_len;
			tmp_offset += tmp_len;
		}
	}

	*len += tmp_total;
	*extent = tmp_extent;
	MASSERT(tmp_offset <= end + 1);
	MRETURN(checksum_value);
}

static __u32 masync_iov_chechsum(const struct iovec *iov,
                                 unsigned long nr_segs,
                                 size_t result_size,
                                 size_t pos,
                                 size_t end,
                                 mtfs_list_t *dirty_extents,
                                 size_t *total,
                                 int do_fill,
                                 mchecksum_type_t type)
{
	__u32 checksum_value = 0;                  /* Checksum value */
	unsigned long tmp_seg = 0;                 /* Segment counter */
	size_t tmp_total = 0;                      /* Calculated length */
	size_t tmp_len = 0;                        /* Length this iter will calculate */
	const struct iovec *tmp_iov = NULL;        /* IOV this iter will calculate */
	size_t offset = pos;                       /* Start offset of extent to calculate */
	struct mtfs_interval *tmp_extent = NULL;
	MENTRY();

	if (!mtfs_list_empty(dirty_extents)) {
		tmp_extent = list_first_entry(dirty_extents,
		                              struct mtfs_interval,
		                              mi_linkage);
	}

	checksum_value = mchecksum_init(type);
	for (tmp_seg = 0; tmp_seg < nr_segs; tmp_seg++) {
		tmp_iov = &iov[tmp_seg];
		MASSERT(tmp_iov->iov_len >= 0);
		if (tmp_total + tmp_iov->iov_len > result_size) {
			tmp_len = result_size - tmp_total;
		} else {
			tmp_len = tmp_iov->iov_len;
		}

		if (tmp_len > 0) {
			MDEBUG("checksum_value = 0x%x, "
			       "offset = %lu, "
			       "len = %lu\n",
			       checksum_value,
			       offset,
			       tmp_len);
			if (tmp_extent) {
				checksum_value = masync_checksum_compute(checksum_value,
				                                         tmp_iov->iov_base,
				                                         tmp_len,
				                                         offset,
				                                         dirty_extents,
				                                         &tmp_extent,
				                                         type);
			} else {
				checksum_value = mchecksum_compute(checksum_value,
				                                   tmp_iov->iov_base,
				                                   tmp_len,
				                                   type);
			}
			MDEBUG("checksum_value = 0x%x\n", checksum_value);
			offset += tmp_len;
		}

 		tmp_total += tmp_iov->iov_len;
 		if (tmp_total >= result_size) {
 			break;
 		}
	}

	*total = result_size;
	if (do_fill) {
		/*
		 * Sometimes, async branch may short read,
		 * but there are some dirty extexts between
		 * [pos + result_size, pos + rw_size - 1],
		 * we should fill the holes whith zero.
		 */
		MDEBUG("checksum_value = 0x%x, "
		       "total = %lu, "
		       "offset = %lu, "
		       "end = %lu\n",
		       checksum_value,
		       *total,
		       offset,
		       end);
		if (tmp_extent) {
			checksum_value = masync_checksum_fill(checksum_value,
			                                      total,
			                                      offset,
			                                      end,
	                                                      dirty_extents,
	                                                      &tmp_extent,
	                                                      type);
		}
		MDEBUG("checksum_value = 0x%x\n", checksum_value);
	}

	MRETURN(checksum_value);
}

static int masync_checksum_push(struct mtfs_io *io,
                                ssize_t readable_size,
                                __u32 checksum_value)
{
	struct mtfs_io_checksum *checksum = &io->subject.mi_checksum;
	struct mtfs_io_rw *io_rw = &io->u.mi_rw;
	struct file *file = io_rw->file;
	int ret = 0;
	mtfs_bindex_t global_bindex = mio_bindex(io);
	MENTRY();

	if (checksum->gather.valid) {
		if (checksum->gather.ssize != readable_size) {
			MERROR("read size of branch[%d] of file [%.*s] is different, "
			       "expected %lld, got %lld\n",
			       global_bindex,
			       file->f_dentry->d_name.len,
			       file->f_dentry->d_name.name,
			       checksum->gather.ssize,
			       io->mi_result.ssize);
			ret = 1;
			goto out;
		}
	
		if (checksum->gather.checksum != checksum_value) {
			MERROR("content of branch[%d] of file [%.*s] is different, "
			       "expected 0x%x, got 0x%x\n",
			       global_bindex,
			       file->f_dentry->d_name.len,
			       file->f_dentry->d_name.name,
			       checksum->gather.checksum,
			       checksum_value);
			ret = 1;
			goto out;
		}
	} else {
		checksum->gather.valid = 1;
		checksum->gather.ssize = readable_size;
		checksum->gather.checksum = checksum_value;
	}
out:
	MRETURN(ret);
}


static int masync_checksum_need_append(struct mtfs_io_checksum *checksum,
                                       size_t offset)
{
	int ret = 0;
	struct mtfs_interval *tmp_extent = NULL;
	MENTRY();

	if (mtfs_list_empty(&checksum->dirty_extents)) {
		ret = 1;
	} else {
		tmp_extent = mtfs_list_entry((&checksum->dirty_extents)->prev,
		                             struct mtfs_interval,
		                             mi_linkage);
		if (tmp_extent->mi_node.in_extent.end < offset) {
			ret = 1;
		}
	}

	MRETURN(ret);
}

/* Called holding bucket->mab_lock */
static void masync_checksum_branch_primary(struct mtfs_io *io)
{
	struct mtfs_io_rw *io_rw = &io->u.mi_rw;
	struct mtfs_io_checksum *checksum = &io->subject.mi_checksum;
	struct file *file = io_rw->file;
	struct dentry *dentry = file->f_dentry;
	struct inode *inode = dentry->d_inode;
	struct masync_bucket *bucket = mtfs_i2bucket(inode);
	struct mtfs_interval_node_extent tmp_interval;
	loff_t pos = *(io_rw->ppos);
	enum mtfs_interval_iter iter = MTFS_INTERVAL_ITER_CONT;
	__u32 checksum_value = 0;
	struct mtfs_interval *tmp_extent = NULL;
	size_t total = 0;
	int ret = 0;
	MENTRY();

	/* 
	 * There should no be any dirty extent between
	 * [pos + result_size, EOF]
	 */
	MASSERT(io->mi_result.ssize >= 0);
	MASSERT(io->mi_result.ssize <= io_rw->rw_size);
	if (io->mi_result.ssize < io_rw->rw_size) {
		tmp_interval.start = pos + io->mi_result.ssize;
		tmp_interval.end = MTFS_INTERVAL_EOF;
		iter = mtfs_interval_search(bucket->mab_root,
		                            &tmp_interval,
		                            masync_checksum_overlap_cb,
		                            &tmp_interval);
		if (iter == MTFS_INTERVAL_ITER_STOP) {
			MERROR("unexpected short read\n");
			MERROR("tried [%lu, %lu], "
			       "read [%lu, %lu]\n",
			       pos, pos + io_rw->rw_size - 1,
			       pos, pos + io->mi_result.ssize - 1);
			MBUG();
		}

		/*
		 * Short read check again.
		 * Dirty_extents is between [pos, pos + rw_size - 1].
		 * So we need only to check the end of last extent,
		 * to insure no diry extents between [pos + result_size, pos + rw_size - 1].
		 */
		if (!mtfs_list_empty(&checksum->dirty_extents)) {
			tmp_extent = mtfs_list_entry((&checksum->dirty_extents)->prev,
			                             struct mtfs_interval,
			                             mi_linkage);
			if (tmp_extent->mi_node.in_extent.end > pos + io->mi_result.ssize - 1) {
				MERROR("unexpected short read\n");
				MERROR("tried [%lu, %lu], "
				       "read [%lu, %lu], "
				       "dirty [%lu, %lu]\n",
				       pos, pos + io_rw->rw_size - 1,
				       pos, pos + io->mi_result.ssize - 1,
				       tmp_extent->mi_node.in_extent.start,
				       tmp_extent->mi_node.in_extent.end);
				masync_dirty_extets_dump(&checksum->dirty_extents);
				masync_extets_dump(bucket);
				MBUG();
			}
		}
	}

	checksum_value = masync_iov_chechsum(io_rw->iov,
	                                     io_rw->nr_segs,
	                                     io->mi_result.ssize,
	                                     pos,
	                                     pos + io_rw->rw_size - 1,
	                                     &checksum->dirty_extents,
	                                     &total,
	                                     0,
	                                     checksum->type);
	MASSERT(total == io->mi_result.ssize);

	ret = masync_checksum_push(io, total, checksum_value);
	if (ret) {
		MERROR("tried [%lu, %lu], "
		       "read [%lu, %lu]\n",
		       pos, pos + io_rw->rw_size - 1,
		       pos, pos + io->mi_result.ssize - 1);
		masync_dirty_extets_dump(&checksum->dirty_extents);
		masync_extets_dump(bucket);
		mtfs_inode_size_dump(inode);
		MBUG();
	}
	_MRETURN();
}

/* Called holding bucket->mab_lock */
static void masync_checksum_branch_secondary(struct mtfs_io *io)
{
	struct mtfs_io_rw *io_rw = &io->u.mi_rw;
	struct file *file = io_rw->file;
	struct dentry *dentry = file->f_dentry;
	struct inode *inode = dentry->d_inode;
	struct masync_bucket *bucket = mtfs_i2bucket(inode);
	struct mtfs_io_checksum *checksum = &io->subject.mi_checksum;
	loff_t pos = *(io_rw->ppos);
	__u32 checksum_value = 0;
	size_t i = 0;
	size_t total = 0;
	size_t tmp_len = 0;
	struct mtfs_interval_node_extent tmp_interval;
	enum mtfs_interval_iter iter = MTFS_INTERVAL_ITER_CONT;
	char data[1] = {0};
	int ret = 0;
	MENTRY();

	MASSERT(io->mi_result.ssize >= 0);
	MASSERT(io->mi_result.ssize <= io_rw->rw_size);
	checksum_value = masync_iov_chechsum(io_rw->iov,
	                                     io_rw->nr_segs,
	                                     io->mi_result.ssize,
	                                     pos,
	                                     pos + io_rw->rw_size - 1,
	                                     &checksum->dirty_extents,
	                                     &total,
	                                     1,
	                                     checksum->type);

	MASSERT(total >= io->mi_result.ssize);
	MASSERT(total <= io_rw->rw_size);

	
	if (total < io_rw->rw_size) {
		/*
		 * Sometimes, async branch may short read,
		 * but there are some dirty extexts between
		 * [pos + rw_size, EOF],
		 * we should fill the holes whith zero.
		 */
	    	MASSERT(masync_checksum_need_append(checksum, pos + total));
		/* Sometimes need to add zero to tail */
		tmp_interval.start = pos + io_rw->rw_size;
		tmp_interval.end = MTFS_INTERVAL_EOF;
		iter = mtfs_interval_search(bucket->mab_root,
		                            &tmp_interval,
		                            masync_checksum_overlap_cb,
		                            &tmp_interval);
		if (iter == MTFS_INTERVAL_ITER_STOP) {
			tmp_len = io_rw->rw_size - total;
			for (i = 0; i < tmp_len; i++) {
				checksum_value = mchecksum_compute(checksum_value,
				                                   data,
				                                   1,
				                                   checksum->type);
			}
			total += tmp_len;
		}
	}
	MASSERT(total >= io->mi_result.ssize);
	MASSERT(total <= io_rw->rw_size);

	ret = masync_checksum_push(io, total, checksum_value);
	if (ret) {
		MERROR("tried [%lu, %lu], "
		       "read [%lu, %lu]\n",
		       pos, pos + io_rw->rw_size - 1,
		       pos, pos + io->mi_result.ssize - 1);
		masync_dirty_extets_dump(&checksum->dirty_extents);
		MBUG();
	}

	_MRETURN();
}

static void masync_iter_check_readv(struct mtfs_io *io)
{
	struct mtfs_io_rw *io_rw = &io->u.mi_rw;
	struct file *file = io_rw->file;
	struct dentry *dentry = file->f_dentry;
	struct inode *inode = dentry->d_inode;
	MENTRY();

	if ((io->mi_flags & MTFS_OPERATION_SUCCESS) &&
	    mtfs_dev2checksum(mtfs_i2dev(inode))) {
		if (io->mi_bindex == io->mi_bnum - 1) {
			masync_checksum_branch_primary(io);
		} else {
			masync_checksum_branch_secondary(io);
		}
	}

	_MRETURN();
}

static int test_fid(struct mtfs_lowerfs *lowerfs,
		     struct super_block *sb,
		     __u64 fid)
{
	struct inode *inode = NULL;
	struct dentry *dentry = NULL;
	int ret = 0;
	MENTRY();

	inode = mlowerfs_iget(lowerfs, sb, fid);
	if (IS_ERR(inode)) {
		MERROR("failed to get inode from fid %llu\n", fid);
		ret = PTR_ERR(inode);
		goto out;
	}

	dentry = d_obtain_alias(inode);
	if (IS_ERR(dentry)) {
		MERROR("failed to get dentry from inode %llu\n", fid);
		ret = PTR_ERR(dentry);
		goto out;
	}

	MERROR("get dentry of %llu: [%.*s]\n",
	       fid,
	       dentry->d_name.len,
	       dentry->d_name.name);

	dput(dentry);
out:
	MRETURN(ret);
}

static int masync_set_async(struct inode *inode, struct mlog_cookie *cookies)
{
	struct super_block *sb = inode->i_sb;
	mtfs_bindex_t bindex = 0;
	mtfs_bindex_t bnum = mtfs_s2bnum(sb);
	struct mtfs_device *device = mtfs_s2dev(sb);
	int ret = 0;
	struct mtfs_lowerfs *lowerfs = NULL;
	struct mlog_async_rec *rec;
	struct mtfs_run_ctxt *saved;
	struct mtfs_ucred ucred = { 0 };
	MENTRY();

	MTFS_ALLOC_PTR(rec);
	if (rec == NULL) {
		MERROR("not enough memory\n");
		goto out;
	}

	MTFS_ALLOC_PTR(saved);
	if (saved == NULL) {
		MERROR("not enough memory\n");
		goto out_free_rec;
	}

	rec->mas_hdr.mrh_len = sizeof(struct mlog_async_rec);
	rec->mas_hdr.mrh_type =  MLOG_ASYNC_MAGIC;

        /* the owner of log file should always be root */
        cap_raise(ucred.luc_cap, CAP_SYS_RESOURCE);

	/*
	 * Do not use mtfs_s2bops,
	 * since mtfs_s2dev is inited in mtfs_init_super()
	 */
	for (bindex = 0; bindex < bnum; bindex++) {
		lowerfs = mtfs_dev2blowerfs(device, bindex);
		if (!lowerfs->ml_trans_support) {
			continue;
		}
		if (!mtfs_i2branch(inode, bindex)) {
			continue;
		}
		MASSERT(mtfs_s2blogctxt(sb, bindex));
		MASSERT(mtfs_s2bcathandle(sb, bindex));
		rec->mas_fid = mtfs_i2branch(inode, bindex)->i_ino;
		if (0)
			test_fid(mtfs_s2blowerfs(sb, bindex),
				 mtfs_s2branch(sb, bindex),
				 rec->mas_fid);
		MERROR("set async %llu\n", rec->mas_fid);
		mtfs_push_ctxt(saved, &mtfs_s2bctxt(sb, bindex), &ucred);
		ret = mlog_cat_add_rec(mtfs_s2bcathandle(sb, bindex),
				       &rec->mas_hdr,
				       &cookies[bindex],
				       NULL);
		mtfs_pop_ctxt(saved, &mtfs_s2bctxt(sb, bindex), &ucred);
		if (ret != 1) {
			MERROR("failed to write mlog record: %d\n", ret);
			goto out_clear;
		}
		ret = 0;
	}
	goto out_free_saved;
out_clear:
	for (; bindex >= 0; bindex--) {
		lowerfs = mtfs_dev2blowerfs(device, bindex);
		if (!lowerfs->ml_trans_support) {
			continue;
		}
		if (!mtfs_i2branch(inode, bindex)) {
			continue;
		}
	        ret = mlog_cat_cancel_records(mtfs_s2bcathandle(sb, bindex),
	        			      1,
	        			      &cookies[bindex]);
		if (ret) {
			MERROR("cancel mlog record[%d] failed: %d\n", bindex, ret);
		}
	}
out_free_saved:
	MTFS_FREE_PTR(saved);
out_free_rec:
	MTFS_FREE_PTR(rec);
out:
	MRETURN(ret);
}

static int masync_clear_async(struct inode *inode,
			      struct mlog_cookie *cookies)
{
	struct super_block *sb = inode->i_sb;
	mtfs_bindex_t bindex = 0;
	mtfs_bindex_t bnum = mtfs_s2bnum(sb);
	struct mtfs_device *device = mtfs_s2dev(sb);
	int ret = 0;
	struct mtfs_lowerfs *lowerfs = NULL;
	MENTRY();

	/*
	 * Do not use mtfs_s2bops,
	 * since mtfs_s2dev is inited in mtfs_init_super()
	 */
	for (bindex = 0; bindex < bnum; bindex++) {
		lowerfs = mtfs_dev2blowerfs(device, bindex);
		if (!lowerfs->ml_trans_support) {
			continue;
		}
		if (!mtfs_i2branch(inode, bindex)) {
			continue;
		}
		MASSERT(mtfs_s2blogctxt(sb, bindex));
		MASSERT(mtfs_s2bcathandle(sb, bindex));
		ret = mlog_cat_cancel_records(mtfs_s2bcathandle(sb, bindex),
	        			      1,
	        			      &cookies[bindex]);
		if (ret) {
			MERROR("cancel mlog record[%d] failed: %d\n", bindex, ret);
		}
	}

	MRETURN(ret);
}

static void masync_io_iter_start_writev(struct mtfs_io *io)
{
	struct mtfs_io_rw *io_rw = &io->u.mi_rw;
	struct mtfs_interval_node_extent extent;
	struct file *file = io_rw->file;
	struct dentry *dentry = file->f_dentry;
	struct masync_extent *async_extent = NULL;
	int ret = 0;
	struct mlog_cookie *cookies = NULL;
	MENTRY();

	MASSERT(io->mi_type == MIOT_WRITEV);
	MASSERT(io->mi_bindex == 0);

	MTFS_ALLOC(cookies, sizeof(struct mlog_cookie) * mtfs_f2bnum(file));
	if (cookies == NULL) {
		MERROR("not enough memory\n");
		goto out;
	}
	/*
	 * TODO: sign the file async,
	 * since the server may crash immediately after branch write completes. 
	 */
	ret = masync_set_async(file->f_dentry->d_inode, cookies);
	if (ret) {
		MERROR("failed to set async of file [%.*s], ret = %d\n",
		       dentry->d_name.len, dentry->d_name.name,
		       ret);
		goto out_free_cookies;
	}

	ret = masync_bucket_add_start(file->f_dentry->d_inode,
	                              &async_extent);
	if (ret) {
		MERROR("failed to add extent to bucket of [%.*s], ret = %d\n",
		       dentry->d_name.len, dentry->d_name.name,
		       ret);
		goto out_clear_aync;
	}
	mio_iter_start_rw(io);

	if (!(io->mi_flags & MTFS_OPERATION_SUCCESS)) {
		/* Only neccessary when succeeded */
		goto out_bucket_abort;
	}

	if (io->mi_result.ssize == 0) {
		goto out_bucket_abort;
	}

	/* Get range from nothing but result because of O_APPEND */
	extent.start = io_rw->pos_tmp - io->mi_result.ssize;
	extent.end = io_rw->pos_tmp - 1;
	MASSERT(extent.start >= 0);
	MASSERT(extent.start <= extent.end);

	if (!masync_bucket_fvalid(file)) {
		/* TODO: reconstruct the bucket */
		goto out_bucket_abort;
	}

	masync_bucket_add_end(file, &extent, async_extent);

	goto out_free_cookies;
out_bucket_abort:
	masync_bucket_add_abort(file, &extent, async_extent);
out_clear_aync:
	ret = masync_clear_async(file->f_dentry->d_inode, cookies);
	if (ret) {
		MERROR("failed to clear async of file [%.*s], ret = %d\n",
		       dentry->d_name.len, dentry->d_name.name,
		       ret);
	}
out_free_cookies:
	MTFS_FREE(cookies, sizeof(struct mlog_cookie) * mtfs_f2bnum(file));
out:
	_MRETURN();
}

static void masync_io_iter_fini_read_ops(struct mtfs_io *io, int init_ret)
{
	MENTRY();

	MASSERT(io->mi_bindex == 0);
	
	if (unlikely(init_ret)) {
		MBUG();
	}

	io->mi_break = 1;
	_MRETURN();
}

static void masync_io_iter_fini_writev(struct mtfs_io *io, int init_ret)
{
	MENTRY();

	if (unlikely(init_ret)) {
		io->mi_break = 1;
		MBUG();
		goto out;
	}

	io->mi_break = 1;
out:
	_MRETURN();
}


static void masync_io_iter_fini_create_ops(struct mtfs_io *io, int init_ret)
{
	MENTRY();

	if (unlikely(init_ret)) {
		io->mi_break = 1;
		MBUG();
		goto out;
	}

	if ((!(io->mi_flags & MTFS_OPERATION_SUCCESS)) ||
	    (io->mi_bindex == io->mi_bnum - 1)) {
		io->mi_break = 1;
	} else {
		io->mi_bindex++;
	}

out:
	_MRETURN();
}

static void masync_io_iter_fini_unlink_ops(struct mtfs_io *io, int init_ret)
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

static int masync_io_init_readv(struct mtfs_io *io)
{
	struct dentry *dentry = NULL;
	struct inode *inode = NULL;
	int ret = 0;
	MENTRY();

	dentry = io->mi_oplist_dentry;
	if (dentry) {
		inode = dentry->d_inode;
	} else {
		MASSERT(io->mi_oplist_inode);
		inode = io->mi_oplist_inode;
	}
	MASSERT(inode);

	if (mtfs_dev2checksum(mtfs_i2dev(inode))) {
		ret = mio_init_oplist(io, &mtfs_oplist_reverse);
		io->subject.mi_checksum.type = mchecksum_type_select();	
	} else {
		ret = mio_init_oplist(io, &mtfs_oplist_equal);
	}

	MRETURN(ret);
}

static int masync_io_init_create_ops(struct mtfs_io *io)
{
	return mio_init_oplist(io, &mtfs_oplist_master);
}

static int masync_io_init_unlink_ops(struct mtfs_io *io)
{
	return mio_init_oplist(io, &mtfs_oplist_equal);
}

static int masync_io_lock_setattr(struct mtfs_io *io)
{
	int ret = 0;
	struct mtfs_io_setattr *io_setattr = &io->u.mi_setattr;
	struct iattr *attr = io_setattr->ia;
	int ia_valid = attr->ia_valid;
	MENTRY();

	/* We only need to get read lock no matter readv, writev or truncate */
	if ((ia_valid & ATTR_SIZE) && 
	    mtfs_dev2checksum(mtfs_d2dev(io_setattr->dentry))) {
		io->mi_einfo.mode = MLOCK_MODE_CLEAN;
		ret = mio_lock_mlock(io);
	}

	MRETURN(ret);
}

static void masync_io_unlock_setattr(struct mtfs_io *io)
{
	struct mtfs_io_setattr *io_setattr = &io->u.mi_setattr;
	struct iattr *attr = io_setattr->ia;
	int ia_valid = attr->ia_valid;
	MENTRY();

	if ((ia_valid & ATTR_SIZE) &&
	    mtfs_dev2checksum(mtfs_d2dev(io_setattr->dentry))) {
		mio_unlock_mlock(io);
	}

	_MRETURN();
}

static void masync_iter_start_setattr(struct mtfs_io *io)
{
	struct mtfs_io_setattr *io_setattr = &io->u.mi_setattr;
	struct dentry *dentry = io_setattr->dentry;
	struct inode *inode = dentry->d_inode;
	struct masync_bucket *bucket = mtfs_i2bucket(inode);
	struct iattr *attr = io_setattr->ia;
	int ia_valid = attr->ia_valid;
	int ret = 0;
	MENTRY();

	mio_iter_start_setattr(io);

	if ((ia_valid & ATTR_SIZE) &&
	    io->mi_bindex == 0 &&
	    io->mi_flags & MTFS_OPERATION_SUCCESS) {
	    	/* Truncate dirty extents */
		/* This would not fail. */
		io->mi_result.ret = masync_bucket_truncate(bucket, attr->ia_size);
		if (io->mi_result.ret) {
			MERROR("failed to truncate extent of [%.*s], "
			       "ret = %d\n",
			       dentry->d_name.len, dentry->d_name.name,
			       ret);
			MBUG();
		}
	}

	_MRETURN();
}

static int masync_io_rw_lock(struct mtfs_io *io)
{
	int ret = 0;
	struct mtfs_io_rw *io_rw = &io->u.mi_rw;
	struct file *file = io_rw->file;
	struct dentry *dentry = file->f_dentry;
	struct inode *inode = dentry->d_inode;
	struct masync_bucket *bucket = mtfs_i2bucket(inode);
	MENTRY();

	if (mtfs_dev2checksum(mtfs_f2dev(file))) {
		if (io->mi_type == MIOT_WRITEV) {
			io->mi_einfo.mode = MLOCK_MODE_DIRTY;
		} else {
			/* Need to check file, so get write lock */
			io->mi_einfo.mode = MLOCK_MODE_CHECK;
		}
		ret = mio_lock_mlock(io);
		if (ret) {
			MERROR("failed to lock extent\n");
			goto out;
		}

		if (io->mi_type == MIOT_READV) {
			/* Lock to prevent race of file sync */
			down(&bucket->mab_lock);
			masync_dirty_extets_build(io);
		}
	}

out:
	MRETURN(ret);
}

static void masync_io_rw_unlock(struct mtfs_io *io)
{
	struct mtfs_io_rw *io_rw = &io->u.mi_rw;
	struct file *file = io_rw->file;
	struct dentry *dentry = file->f_dentry;
	struct inode *inode = dentry->d_inode;
	struct masync_bucket *bucket = mtfs_i2bucket(inode);
	MENTRY();

	if (mtfs_dev2checksum(mtfs_f2dev(file))) {
		if (io->mi_type == MIOT_READV) {	
			up(&bucket->mab_lock);
		}

		mio_unlock_mlock(io);
	}

	_MRETURN();
}

static void masync_iter_end_readv(struct mtfs_io *io)
{
	mio_iter_end_oplist(io);
	masync_iter_check_readv(io);
}

const struct mtfs_io_operations masync_io_ops[] = {
	[MIOT_CREATE] = {
		.mio_init       = masync_io_init_create_ops,
		.mio_fini       = mio_fini_oplist,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mio_iter_start_create,
		.mio_iter_end   = mio_iter_end_oplist,
		.mio_iter_fini  = masync_io_iter_fini_create_ops,
	},
	[MIOT_LINK] = {
		.mio_init       = masync_io_init_create_ops,
		.mio_fini       = mio_fini_oplist,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mio_iter_start_link,
		.mio_iter_end   = mio_iter_end_oplist,
		.mio_iter_fini  = masync_io_iter_fini_create_ops,
	},
	[MIOT_UNLINK] = {
		.mio_init       = masync_io_init_unlink_ops,
		.mio_fini       = mio_fini_oplist,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mio_iter_start_unlink,
		.mio_iter_end   = mio_iter_end_oplist,
		.mio_iter_fini  = masync_io_iter_fini_unlink_ops,
	},
	[MIOT_MKDIR] = {
		.mio_init       = masync_io_init_create_ops,
		.mio_fini       = mio_fini_oplist,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mio_iter_start_mkdir,
		.mio_iter_end   = mio_iter_end_oplist,
		.mio_iter_fini  = masync_io_iter_fini_create_ops,
	},
	[MIOT_RMDIR] = {
		.mio_init       = masync_io_init_unlink_ops,
		.mio_fini       = mio_fini_oplist,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mio_iter_start_rmdir,
		.mio_iter_end   = mio_iter_end_oplist,
		.mio_iter_fini  = masync_io_iter_fini_unlink_ops,
	},
	[MIOT_MKNOD] = {
		.mio_init       = masync_io_init_create_ops,
		.mio_fini       = mio_fini_oplist,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mio_iter_start_mknod,
		.mio_iter_end   = mio_iter_end_oplist,
		.mio_iter_fini  = masync_io_iter_fini_create_ops,
	},
	[MIOT_RENAME] = {
		.mio_init       = masync_io_init_create_ops,
		.mio_fini       = mio_fini_oplist,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mio_iter_start_rename,
		.mio_iter_end   = mio_iter_end_oplist,
		.mio_iter_fini  = masync_io_iter_fini_create_ops,
	},
	[MIOT_SYMLINK] = {
		.mio_init       = masync_io_init_create_ops,
		.mio_fini       = mio_fini_oplist,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mio_iter_start_symlink,
		.mio_iter_end   = mio_iter_end_oplist,
		.mio_iter_fini  = masync_io_iter_fini_create_ops,
	},
	[MIOT_READLINK] = {
		.mio_init       = NULL,
		.mio_fini       = NULL,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mio_iter_start_readlink,
		.mio_iter_end   = NULL,
		.mio_iter_fini  = masync_io_iter_fini_read_ops,
	},
	[MIOT_PERMISSION] = {
		.mio_init       = NULL,
		.mio_fini       = NULL,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mio_iter_start_permission,
		.mio_iter_end   = NULL,
		.mio_iter_fini  = masync_io_iter_fini_read_ops,
	},
	[MIOT_READV] = {
		.mio_init       = masync_io_init_readv,
		.mio_fini       = mio_fini_oplist_noupdate,
		.mio_lock       = masync_io_rw_lock,
		.mio_unlock     = masync_io_rw_unlock,
		.mio_iter_init  = mio_iter_init_rw,
		.mio_iter_start = mio_iter_start_rw,
		.mio_iter_end   = masync_iter_end_readv,
		.mio_iter_fini  = mio_iter_fini_read_ops,
	},
	[MIOT_WRITEV] = {
		.mio_init       = masync_io_init_create_ops,
		.mio_fini       = mio_fini_oplist,
		.mio_lock       = masync_io_rw_lock,
		.mio_unlock     = masync_io_rw_unlock,
		.mio_iter_init  = mio_iter_init_rw,
		.mio_iter_start = masync_io_iter_start_writev,
		.mio_iter_end   = mio_iter_end_oplist,
		.mio_iter_fini  = masync_io_iter_fini_writev,
	},
	[MIOT_GETATTR] = {
		.mio_init       = NULL,
		.mio_fini       = NULL,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mio_iter_start_getattr,
		.mio_iter_end   = NULL,
		.mio_iter_fini  = masync_io_iter_fini_read_ops,
	},
	[MIOT_SETATTR] = {
		.mio_init       = masync_io_init_create_ops,
		.mio_fini       = mio_fini_oplist,
		.mio_lock       = masync_io_lock_setattr,
		.mio_unlock     = masync_io_unlock_setattr,
		.mio_iter_init  = NULL,
		.mio_iter_start = masync_iter_start_setattr,
		.mio_iter_end   = mio_iter_end_oplist,
		.mio_iter_fini  = masync_io_iter_fini_create_ops,
	},
	[MIOT_GETXATTR] = {
		.mio_init       = NULL,
		.mio_fini       = NULL,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mio_iter_start_getxattr,
		.mio_iter_end   = NULL,
		.mio_iter_fini  = masync_io_iter_fini_read_ops,
	},
	[MIOT_SETXATTR] = {
		.mio_init       = masync_io_init_create_ops,
		.mio_fini       = mio_fini_oplist,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mio_iter_start_setxattr,
		.mio_iter_end   = mio_iter_end_oplist,
		.mio_iter_fini  = masync_io_iter_fini_create_ops,
	},
	[MIOT_REMOVEXATTR] = {
		.mio_init       = masync_io_init_unlink_ops,
		.mio_fini       = mio_fini_oplist,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mio_iter_start_removexattr,
		.mio_iter_end   = mio_iter_end_oplist,
		.mio_iter_fini  = masync_io_iter_fini_unlink_ops,
	},
	[MIOT_LISTXATTR] = {
		.mio_init       = NULL,
		.mio_fini       = NULL,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mio_iter_start_listxattr,
		.mio_iter_end   = NULL,
		.mio_iter_fini  = masync_io_iter_fini_read_ops,
	},
	[MIOT_READDIR] = {
		.mio_init       = NULL,
		.mio_fini       = NULL,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mio_iter_start_readdir,
		.mio_iter_end   = NULL,
		.mio_iter_fini  = masync_io_iter_fini_read_ops,
	},
	[MIOT_OPEN] = {
		.mio_init       = masync_io_init_create_ops,
		.mio_fini       = mio_fini_oplist,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mio_iter_start_open,
		.mio_iter_end   = mio_iter_end_oplist,
		.mio_iter_fini  = masync_io_iter_fini_create_ops,
	},
	[MIOT_IOCTL_WRITE] = {
		.mio_init       = masync_io_init_create_ops,
		.mio_fini       = mio_fini_oplist,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mio_iter_start_ioctl,
		.mio_iter_end   = mio_iter_end_oplist,
		.mio_iter_fini  = masync_io_iter_fini_create_ops,
	},
	[MIOT_IOCTL_READ] = {
		.mio_init       = NULL,
		.mio_fini       = NULL,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mio_iter_start_ioctl,
		.mio_iter_end   = NULL,
		.mio_iter_fini  = masync_io_iter_fini_read_ops,
	},
	[MIOT_WRITEPAGE] = {
		.mio_init       = masync_io_init_create_ops,
		.mio_fini       = mio_fini_oplist,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mio_iter_start_writepage,
		.mio_iter_end   = mio_iter_end_oplist,
		.mio_iter_fini  = masync_io_iter_fini_create_ops,
	},
	[MIOT_READPAGE] = {
		.mio_init       = NULL,
		.mio_fini       = NULL,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mio_iter_start_readpage,
		.mio_iter_end   = NULL,
		.mio_iter_fini  = masync_io_iter_fini_read_ops,
	},
};
EXPORT_SYMBOL(masync_io_ops);
