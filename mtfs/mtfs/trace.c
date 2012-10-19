/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <linux/module.h>
#include <linux/sched.h>
#include <mtfs_io.h>
#include <mtfs_file.h>
#include "main_internal.h"
#include "io_internal.h"

static void _mtfs_io_iter_start_rw(struct mtfs_io *io)
{
	struct mtfs_io_rw *io_rw = &io->u.mi_rw;
	int is_write = 0;
	MENTRY();

	is_write = (io->mi_type == MIT_WRITEV) ? 1 : 0;
	io->mi_result.size = mtfs_file_rw_branch(is_write,
	                                         io_rw->file,
	                                         io_rw->iov,
	                                         io_rw->nr_segs,
	                                         io_rw->ppos,
	                                         io->mi_bindex);
	if (io->mi_result.size > 0) {
		/* TODO: this check is weak */
		io->mi_successful = 1;
	} else {
		io->mi_successful = 0;
	}

	_MRETURN();
}

static void mtrace_io_iter_start_rw(struct mtfs_io *io)
{
	struct mtfs_io_trace *io_trace = &io->subject.mi_trace;
	MENTRY();

	if (io->mi_bindex != io->mi_bnum - 1) {
		do_gettimeofday(&io_trace->start);
		_mtfs_io_iter_start_rw(io);
		do_gettimeofday(&io_trace->end);
	} else {
		/* Trace branch */
		
	}
	_MRETURN();
}

static void mtrace_io_iter_fini_rw(struct mtfs_io *io, int init_ret)
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

	if (io->mi_bindex == io->mi_bnum - 1) {
	    	io->mi_break = 1;
	} else {
		io->mi_bindex++;
	}

out:
	_MRETURN();
}

const struct mtfs_io_operations mtrace_io_ops[] = {
	[MIT_READV] = {
		.mio_init       = NULL,
		.mio_fini       = NULL,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mtrace_io_iter_start_rw,
		.mio_iter_end   = NULL,
		.mio_iter_fini  = mtrace_io_iter_fini_rw,
	},
	[MIT_WRITEV] = {
		.mio_init       = NULL,
		.mio_fini       = NULL,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mtrace_io_iter_start_rw,
		.mio_iter_end   = NULL,
		.mio_iter_fini  = mtrace_io_iter_fini_rw,
	},
};

static int mtrace_io_init_rw(struct mtfs_io *io, int is_write,
                             struct file *file, const struct iovec *iov,
                             unsigned long nr_segs, loff_t *ppos, size_t rw_size)
{
	int ret = 0;
	mtfs_io_type_t type;
	struct mtfs_io_rw *io_rw = &io->u.mi_rw;
	MENTRY();

	type = is_write ? MIT_WRITEV : MIT_READV;
	io->mi_ops = &mtrace_io_ops[type];
	io->mi_type = type;
	io->mi_oplist_dentry = file->f_dentry;
	io->mi_bindex = 0;
	io->mi_bnum = mtfs_f2bnum(file);
	io->mi_break = 0;
	io->mi_oplist_dentry = file->f_dentry;

	io_rw->file = file;
	io_rw->iov = iov;
	io_rw->nr_segs = nr_segs;
	io_rw->ppos = ppos;
	io_rw->iov_length = sizeof(*iov) * nr_segs;

	MRETURN(ret);
}

static ssize_t mtrace_file_rw(int is_write, struct file *file, const struct iovec *iov,
                              unsigned long nr_segs, loff_t *ppos)
{
	ssize_t size = 0;
	int ret = 0;
	struct mtfs_io *io = NULL;
	size_t rw_size = 0;
	MENTRY();

	MTFS_SLAB_ALLOC_PTR(io, mtfs_io_cache);
	if (io == NULL) {
		MERROR("not enough memory\n");
		size = -ENOMEM;
		goto out;
	}

	ret = mtrace_io_init_rw(io, is_write, file, iov, nr_segs, ppos, rw_size);
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
ssize_t mtrace_file_readv(struct file *file, const struct iovec *iov,
                          unsigned long nr_segs, loff_t *ppos)
{
	ssize_t size = 0;
	MENTRY();

	size = mtrace_file_rw(READ, file, iov, nr_segs, ppos);

	MRETURN(size);
}
EXPORT_SYMBOL(mtrace_file_readv);

ssize_t mtrace_file_read(struct file *file, char __user *buf, size_t len,
                         loff_t *ppos)
{
	struct iovec local_iov = { .iov_base = (void __user *)buf,
	                           .iov_len = len };
	return mtrace_file_readv(file, &local_iov, 1, ppos);
}
EXPORT_SYMBOL(mtrace_file_read);

ssize_t mtrace_file_writev(struct file *file, const struct iovec *iov,
                        unsigned long nr_segs, loff_t *ppos)
{
	ssize_t size = 0;
	MENTRY();

	size = mtrace_file_rw(WRITE, file, iov, nr_segs, ppos);

	MRETURN(size);
}
EXPORT_SYMBOL(mtrace_file_writev);
#else /* !HAVE_FILE_READV */
ssize_t mtrace_file_aio_read(struct kiocb *iocb, const struct iovec *iov,
                             unsigned long nr_segs, loff_t pos)
{
	ssize_t size = 0;
	struct file *file = iocb->ki_filp;
	MENTRY();

	size = mtrace_file_rw(READ, file, iov, nr_segs, &pos);
	if (size > 0) {
		iocb->ki_pos = pos + size;
	}

	MRETURN(size);
}
EXPORT_SYMBOL(mtrace_file_aio_read);

ssize_t mtrace_file_aio_write(struct kiocb *iocb, const struct iovec *iov,
                              unsigned long nr_segs, loff_t pos)
{
	ssize_t size = 0;
	struct file *file = iocb->ki_filp;
	MENTRY();

	size = mtrace_file_rw(WRITE, file, iov, nr_segs, &pos);
	if (size > 0) {
		iocb->ki_pos = pos + size;
	}

	MRETURN(size);
}
EXPORT_SYMBOL(mtrace_file_aio_write);

#endif /* !HAVE_FILE_READV */
