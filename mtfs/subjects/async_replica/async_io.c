/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <linux/module.h>
#include <mtfs_io.h>
#include <mtfs_file.h>
#include <mtfs_dentry.h>
#include <mtfs_inode.h>
#include <mtfs_device.h>
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

	if (io->mi_bindex == 0) {
		/*
		 * TODO: sign the file async,
		 * since I may crash immediately after write a branch. 
		 */
		mio_iter_start_rw(io);
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
	} else {
		struct masync_bucket *bucket = mtfs_i2bucket(inode);

		MASSERT(io->mi_type == MIOT_READV);
		/* Not append write, so it is ok to get extent like this.
		 * TODO: probably unnecessary length here.
		 */
		extent.start = *(io_rw->ppos);
		extent.end = *(io_rw->ppos) + io_rw->rw_size;
		MASSERT(extent.start >= 0);
		MASSERT(extent.start <= extent.end);
		/* Lock to prevent race of file sync */
		down(&bucket->mab_lock);
		ret = mtfs_interval_is_overlapped(bucket->mab_root, &extent);
		if (!ret) {
			mio_iter_start_rw(io);
		} else {
			io->mi_flags = 0;
		}
		up(&bucket->mab_lock);
		
		/* Do not read it unless checksum or layout changed */
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

	ret = mio_init_oplist(io, &mtfs_oplist_equal);
	if (!ret && mtfs_dev2checksum(mtfs_i2dev(inode))) {
		io->subject.mi_checksum.type = mchecksum_type_select();	
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
	if (ia_valid & ATTR_SIZE) {
		io->mi_einfo.mode = MLOCK_MODE_READ;
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

	if (ia_valid & ATTR_SIZE) {
		mio_unlock_mlock(io);
	}

	_MRETURN();
}

static int masync_io_rw_lock(struct mtfs_io *io)
{
	int ret = 0;
	MENTRY();

	/* We only need to get read lock no matter readv or writev */
	io->mi_einfo.mode = MLOCK_MODE_READ;
	ret = mio_lock_mlock(io);

	MRETURN(ret);
}

static void masync_io_rw_unlock(struct mtfs_io *io)
{
	MENTRY();

	mio_unlock_mlock(io);

	_MRETURN();
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
		.mio_iter_end   = mio_iter_end_readv,
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
		.mio_iter_start = mio_iter_start_setattr,
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
