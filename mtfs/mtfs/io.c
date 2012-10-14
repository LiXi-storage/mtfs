/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <linux/module.h>
#include <mtfs_common.h>
#include <mtfs_device.h>
#include <mtfs_oplist.h>
#include "file_internal.h"
#include "io_internal.h"

static int mtfs_io_init_oplist(struct mtfs_io *io)
{
	int ret = 0;
	struct dentry *dentry = NULL;
	struct inode *inode = NULL;
	struct mtfs_operation_list *oplist = &io->mi_oplist;
	MENTRY();

	dentry = io->mi_oplist_dentry;
	MASSERT(dentry);
	inode = dentry->d_inode;
	MASSERT(inode);

	mtfs_oplist_init(oplist, inode);
	if (oplist->latest_bnum == 0) {
		MERROR("[%.*s] has no valid branch, please check it\n",
		       dentry->d_name.len, dentry->d_name.name);
		if (!(mtfs_i2dev(inode)->no_abort)) {
			ret = -EIO;
		}
	}

	MRETURN(ret);
}

static void mtfs_io_iter_end_oplist(struct mtfs_io *io)
{
	struct mtfs_operation_list *oplist = &io->mi_oplist;
	MENTRY();

	mtfs_oplist_setbranch(oplist, io->mi_bindex,
	                      io->mi_successful,
	                      io->mi_result);

	_MRETURN();
}

static void mtfs_io_fini_oplist(struct mtfs_io *io)
{
	int ret = 0;
	struct dentry *dentry = NULL;
	struct inode *inode = NULL;
	struct mtfs_operation_list *oplist = &io->mi_oplist;
	MENTRY();

	dentry = io->mi_oplist_dentry;
	MASSERT(dentry);
	inode = dentry->d_inode;
	MASSERT(inode);

	mtfs_oplist_merge(oplist);
	io->mi_result = oplist->opinfo->result;
	io->mi_successful = oplist->opinfo->is_suceessful;

	ret = mtfs_oplist_update(inode, oplist);
	if (ret) {
		MERROR("failed to update oplist for [%.*s]\n",
		       dentry->d_name.len, dentry->d_name.name);
		MBUG();
	}

	_MRETURN();
}

static int mtfs_io_lock_mlock(struct mtfs_io *io)
{
	int ret = 0;
	MENTRY();

	io->mi_mlock = mlock_enqueue(io->mi_resource, &io->mi_einfo);
	if (io->mi_mlock == NULL) {
		MERROR("failed to enqueue lock\n");
		ret = -ENOMEM;
	}

	MRETURN(ret);
}

static void mtfs_io_unlock_mlock(struct mtfs_io *io)
{
	MENTRY();

	MASSERT(io->mi_mlock);
	mlock_cancel(io->mi_mlock);

	_MRETURN();
}

static int mtfs_io_iter_init_rw(struct mtfs_io *io)
{
	int ret = 0;
	struct mtfs_io_rw *io_rw = &io->u.mi_rw;
	MENTRY();

	memcpy((char *)io_rw->iov_tmp,
	       (char *)io_rw->iov,
	       sizeof(*(io_rw->iov)) * io_rw->nr_segs);
	io_rw->pos_tmp = *(io_rw->ppos);

	MRETURN(ret);
}

void mtfs_io_iter_start_rw(struct mtfs_io *io)
{
	struct mtfs_io_rw *io_rw = &io->u.mi_rw;
	mtfs_bindex_t global_bindex = io->mi_oplist.op_binfo[io->mi_bindex].bindex;
	int is_write = 0;
	MENTRY();

	is_write = (io->mi_type == MIT_WRITEV) ? 1 : 0;
	io->mi_result.size = mtfs_file_rw_branch(is_write,
	                                         io_rw->file,
	                                         io_rw->iov_tmp,
	                                         io_rw->nr_segs,
	                                         &io_rw->pos_tmp,
	                                         global_bindex);
	if (io->mi_result.size > 0) {
		/* TODO: this check is weak */
		io->mi_successful = 1;
	} else {
		io->mi_successful = 0;
	}

	_MRETURN();
}

static void mtfs_io_iter_fini_readv(struct mtfs_io *io, int init_ret)
{
	MENTRY();

	if (unlikely(init_ret)) {
		if (io->mi_bindex == io->mi_bnum - 1) {
			io->mi_break = 1;
		} else {
			io->mi_bindex++;
		}
		goto out;
	}

	if (io->mi_successful ||
	    io->mi_bindex == io->mi_oplist.latest_bnum - 1) {
	    	io->mi_break = 1;
	} else {
		io->mi_bindex++;
	}

out:
	_MRETURN();
}

static void mtfs_io_iter_fini_writev(struct mtfs_io *io, int init_ret)
{
	MENTRY();

	if (unlikely(init_ret)) {
		if (io->mi_bindex == io->mi_bnum - 1) {
			io->mi_break = 1;
		} else {
			io->mi_bindex++;
		}
		goto out;
	}

	if (io->mi_bindex == io->mi_oplist.latest_bnum - 1) {
		mtfs_oplist_merge(&io->mi_oplist);
		if (io->mi_oplist.success_latest_bnum <= 0) {
			MDEBUG("operation failed for all latest %d branches\n",
			       io->mi_oplist.latest_bnum);
			if (!(mtfs_i2dev(io->mi_oplist_dentry->d_inode)->no_abort)) {
				io->mi_break = 1;
				goto out;
			}
		}
	}

	if (io->mi_bindex == io->mi_bnum - 1) {
		io->mi_break = 1;
	} else {
		io->mi_bindex++;
	}
out:
	_MRETURN();
}

const struct mtfs_io_operations mtfs_io_ops[] = {
	[MIT_READV] = {
		.mio_init       = mtfs_io_init_oplist,
		.mio_fini       = NULL,
		.mio_lock       = mtfs_io_lock_mlock,
		.mio_unlock     = mtfs_io_unlock_mlock,
		.mio_iter_init  = mtfs_io_iter_init_rw,
		.mio_iter_start = mtfs_io_iter_start_rw,
		.mio_iter_end   = NULL,
		.mio_iter_fini  = mtfs_io_iter_fini_readv,
	},
	[MIT_WRITEV] = {
		.mio_init       = mtfs_io_init_oplist,
		.mio_fini       = mtfs_io_fini_oplist,
		.mio_lock       = mtfs_io_lock_mlock,
		.mio_unlock     = mtfs_io_unlock_mlock,
		.mio_iter_init  = mtfs_io_iter_init_rw,
		.mio_iter_start = mtfs_io_iter_start_rw,
		.mio_iter_end   = mtfs_io_iter_end_oplist,
		.mio_iter_fini  = mtfs_io_iter_fini_writev,
	},
};

static int mtfs_io_init(struct mtfs_io *io)
{
	int ret = 0;
	MENTRY();

	MASSERT(io->mi_ops);
	if (io->mi_ops->mio_init) {
		ret = io->mi_ops->mio_init(io);
	}

	MRETURN(ret);
}

static void mtfs_io_fini(struct mtfs_io *io)
{
	MENTRY();

	MASSERT(io->mi_ops);
	if (io->mi_ops->mio_fini) {
		io->mi_ops->mio_fini(io);
	}

	_MRETURN();
}

static int mtfs_io_lock(struct mtfs_io *io)
{
	int ret = 0;
	MENTRY();

	MASSERT(io->mi_ops);
	if (io->mi_ops->mio_lock) {
		ret = io->mi_ops->mio_lock(io);
	}

	MRETURN(ret);
}

static void mtfs_io_unlock(struct mtfs_io *io)
{
	MENTRY();

	MASSERT(io->mi_ops);
	if (io->mi_ops->mio_unlock) {
		io->mi_ops->mio_unlock(io);
	}

	_MRETURN();
}

static int mtfs_io_iter_init(struct mtfs_io *io)
{
	int ret = 0;
	MENTRY();

	MASSERT(io->mi_ops);
	if (io->mi_ops->mio_iter_init) {
		ret = io->mi_ops->mio_iter_init(io);
	}

	MRETURN(ret);
}

static void mtfs_io_iter_start(struct mtfs_io *io)
{
	MENTRY();

	MASSERT(io->mi_ops);
	if (io->mi_ops->mio_iter_start) {
		io->mi_ops->mio_iter_start(io);
	}

	_MRETURN();
}

static void mtfs_io_iter_end(struct mtfs_io *io)
{
	MENTRY();

	MASSERT(io->mi_ops);
	if (io->mi_ops->mio_iter_end) {
		io->mi_ops->mio_iter_end(io);
	}

	_MRETURN();
}

static void mtfs_io_iter_fini(struct mtfs_io *io, int init_ret)
{
	MENTRY();

	MASSERT(io->mi_ops);
	if (io->mi_ops->mio_iter_fini) {
		io->mi_ops->mio_iter_fini(io, init_ret);
	}

	_MRETURN();
}

int mtfs_io_loop(struct mtfs_io *io)
{
	int ret = 0;
	MENTRY();

	ret = mtfs_io_init(io);
	if (ret) {
		MERROR("failed to init io\n");
		goto out;
	}

	ret = mtfs_io_lock(io);
	if (ret) {
		MERROR("failed to lock for io\n");
		goto out_fini;
	}

	do {
		ret = mtfs_io_iter_init(io);
		if (!ret) {
			mtfs_io_iter_start(io);
			mtfs_io_iter_end(io);
		}
		mtfs_io_iter_fini(io, ret);
	} while ((!ret) && (!io->mi_break));

	mtfs_io_unlock(io);
out_fini:
	mtfs_io_fini(io);
out:
	MRETURN(ret);
}
EXPORT_SYMBOL(mtfs_io_loop);
