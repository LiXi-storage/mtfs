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
	struct mtfs_interval_node_extent *interval = (struct mtfs_interval_node_extent *)args;;

	MERROR("[%lu, %lu] overlaped with dirty extent [%lu, %lu]\n",
	       interval->start, interval->end,
	       node->in_extent.start, node->in_extent.end);
	return MTFS_INTERVAL_ITER_STOP;
}

/* Called holding bucket->mab_lock */
static void masync_checksum_branch_primary(struct mtfs_io *io)
{
	struct mtfs_io_rw *io_rw = &io->u.mi_rw;
	//struct mtfs_io_checksum *checksum = &io->subject.mi_checksum;
	struct file *file = io_rw->file;
	struct dentry *dentry = file->f_dentry;
	struct inode *inode = dentry->d_inode;
	struct masync_bucket *bucket = mtfs_i2bucket(inode);
	struct mtfs_interval_node_extent tmp_interval;
	loff_t pos = *(io_rw->ppos);
	enum mtfs_interval_iter iter = MTFS_INTERVAL_ITER_CONT;
	MENTRY();

	/* 
	 * There should no be any dirty extent between
	 * [pos + result_size, pos + rw_size - 1]
	 */
	MASSERT(io->mi_result.ssize >= 0);
	MASSERT(io->mi_result.ssize <= io_rw->rw_size);
	if (io->mi_result.ssize < io_rw->rw_size) {
		tmp_interval.start = pos + io->mi_result.ssize;
		tmp_interval.end = pos + io_rw->rw_size - 1;
		iter = mtfs_interval_search(bucket->mab_root,
		                            &tmp_interval,
		                            masync_checksum_overlap_cb,
		                            &tmp_interval);
		if (iter == MTFS_INTERVAL_ITER_STOP) {
			MERROR("unexpected short read\n");
			MBUG();
		}
	}

	_MRETURN();
}

/* Called holding bucket->mab_lock */
static int masync_checksum_branch_secondary(struct mtfs_io_rw *io_rw,
                                            struct mtfs_io_checksum *checksum,
                                            struct masync_bucket *bucket,
                                            struct mtfs_interval_node_extent *extent)
{
	int ret = 0;
	MENTRY();

	MRETURN(ret);
}

static void masync_iter_check_readv(struct mtfs_io *io)
{
	struct mtfs_io_rw *io_rw = &io->u.mi_rw;
	struct mtfs_io_checksum *checksum = &io->subject.mi_checksum;
	struct file *file = io_rw->file;
	struct dentry *dentry = file->f_dentry;
	struct inode *inode = dentry->d_inode;
	struct masync_bucket *bucket = mtfs_i2bucket(inode);
	struct mtfs_interval_node_extent extent;
	int ret = 0;
	MENTRY();

	if ((io->mi_flags & MTFS_OPERATION_SUCCESS) &&
	    mtfs_dev2checksum(mtfs_i2dev(inode))) {
	    	extent.start = *(io_rw->ppos);
	    	extent.end = *(io_rw->ppos) + io_rw->rw_size;
		if (io->mi_bindex == io->mi_bnum - 1) {
			masync_checksum_branch_primary(io);
		} else {
			ret = masync_checksum_branch_secondary(io_rw,
                                                               checksum,
                                                               bucket,
                                                               &extent);
			if (ret) {
				MERROR("primary checksum error\n");
				MBUG();
			}
		}
	}


	_MRETURN();
}

static void masync_io_iter_start_rw(struct mtfs_io *io)
{
	struct mtfs_io_rw *io_rw = &io->u.mi_rw;
	struct mtfs_interval_node_extent extent;
	struct file *file = io_rw->file;
	struct dentry *dentry = file->f_dentry;
	struct inode *inode = dentry->d_inode;
	mtfs_bindex_t bindex = 0;
	int ret = 0;
	MENTRY();

	mio_iter_start_rw(io);

	if (io->mi_bindex == 0) {
		/*
		 * TODO: sign the file async,
		 * since the server may crash immediately after branch write completes. 
		 */
		if (io->mi_type == MIOT_WRITEV) {
			if (!(io->mi_flags & MTFS_OPERATION_SUCCESS)) {
				/* Only neccessary when succeeded */
				goto out;
			}

			/* Get range from nothing but result because of O_APPEND */
			extent.start = io_rw->pos_tmp - io->mi_result.ssize;
			extent.end = io_rw->pos_tmp - 1;
			MASSERT(extent.start >= 0);
			MASSERT(extent.start <= extent.end);

			/* Set master */
			bindex = 0;
			MASSERT(mtfs_i2branch(inode, bindex));
			ret = mlowerfs_bucket_add(mtfs_i2bbucket(inode, bindex),
			                          &extent);
			if (ret) {
				MASSERT(ret < 0);
				MERROR("failed to add extent to branch[%d] of [%.*s], ret = %d\n",
				       bindex,
				       dentry->d_name.len, dentry->d_name.name,
				       ret);
				io->mi_result.ssize = ret;
				io->mi_flags = 0;
				MBUG();
				goto out;
			}

			/* Set slave */
			bindex = 1;
			if (mtfs_i2branch(inode, bindex) == NULL) {
				MERROR("branch[%d] of [%.*s] is NULL\n",
				       bindex,
				       dentry->d_name.len, dentry->d_name.name);
				ret = -ENOENT;
				goto out;
			}

			ret = mlowerfs_bucket_add(mtfs_i2bbucket(inode, bindex),
			                          &extent);
			if (ret) {
				MERROR("failed to add extent to branch[%d] of [%.*s], ret = %d\n",
				       bindex,
				       dentry->d_name.len, dentry->d_name.name,
				       ret);
				/* TODO: Any other way to sign it? */
				MBUG();
			}

			if (!masync_bucket_fvalid(file)) {
				/* TODO: reconstruct the bucket */
				goto out;
			}

			masync_bucket_add(file, &extent);
			if (ret) {
				MASSERT(ret < 0);
				MERROR("failed to add extent to bucket of [%.*s], ret = %d\n",
				       dentry->d_name.len, dentry->d_name.name,
				       ret);
				/* TODO: invalidate and reconstruct the bucket */
				MBUG();
				io->mi_result.ssize = ret;
				io->mi_flags = 0;
				goto out;
			}
		}
	}

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
	struct mtfs_interval_node_extent interval;
	MENTRY();

	mio_iter_start_setattr(io);

	if ((ia_valid & ATTR_SIZE) &&
	    io->mi_bindex == 0 &&
	    io->mi_flags & MTFS_OPERATION_SUCCESS) {
	    	/* Truncate dirty extents */
		interval.start = attr->ia_size;
		interval.end = MTFS_INTERVAL_EOF;
		/* This would not fail. */
	    	masync_bucket_remove(bucket, &interval);
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
		.mio_iter_start = masync_io_iter_start_rw,
		.mio_iter_end   = masync_iter_end_readv,
		.mio_iter_fini  = mio_iter_fini_read_ops,
	},
	[MIOT_WRITEV] = {
		.mio_init       = masync_io_init_create_ops,
		.mio_fini       = mio_fini_oplist,
		.mio_lock       = masync_io_rw_lock,
		.mio_unlock     = masync_io_rw_unlock,
		.mio_iter_init  = mio_iter_init_rw,
		.mio_iter_start = masync_io_iter_start_rw,
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
