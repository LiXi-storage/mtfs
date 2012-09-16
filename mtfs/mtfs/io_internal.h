/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_IO_INTERNAL_H__
#define __MTFS_IO_INTERNAL_H__

#include <linux/fs.h>
#include <linux/uio.h>
#include <mtfs_oplist.h>
#include <mtfs_lock.h>

enum mtfs_io_type {
	MIT_READV,
	MIT_WRITEV,
};

struct mtfs_io {
	struct mtfs_io_operations *mi_ops;
	enum mtfs_io_type mi_type;
	mtfs_bindex_t mi_bindex;
	mtfs_bindex_t mi_bnum;
	mtfs_operation_result_t mi_result;
	int mi_continue;
	int mi_successful;
	union {
		struct mtfs_io_rw {
			struct file *file;
			const struct iovec *iov;
			unsigned long nr_segs;
			loff_t *ppos;
			ssize_t ret;

			int is_write;
			struct iovec *iov_tmp;
			loff_t pos_tmp;
		} mi_rw;
	} u;
	/*
	 * Inited when ->mio_init
	 * Set when ->mio_iter_end
	 * Checked when ->mio_fini
	 */
	struct mtfs_operation_list mi_oplist;
	struct dentry *mi_oplist_dentry;
	/*
	 * Locked when ->mio_lock
	 * Unlocked when ->mio_unlock
	 * TODO: alloc with io
	 */
	struct mlock *mi_mlock;
	struct mlock_resource *mi_resource;
	struct mlock_enqueue_info mi_einfo;
};

struct mtfs_io_operations {
	/* Prepare io */
	int (*mio_init) (struct mtfs_io *io);
	/*
	 * Finalize io.
	 * Determin whether this operation is successful.
	 */
	void (*mio_fini) (struct mtfs_io *io);
	/* Collect locks */
	int (*mio_lock) (struct mtfs_io *io);
	/* Finalize unlocking */
	void (*mio_unlock) (struct mtfs_io *io);
	/*
	 * Prepare io iteration.
	 * If failed, should not call ->mio_iter_run() 
	 */
	int (*mio_iter_init) (struct mtfs_io *io);
	/*
	 * Start io iteration.
	 * Do not return any result, is saved it in ->mi_result
	 */
	void (*mio_iter_start) (struct mtfs_io *io);
	/*
	 * End io iteration.
	 */
	void (*mio_iter_end) (struct mtfs_io *io);
	/*
	 * Finalize io iteration.
	 * Determin continue to iter or not.
	 * If continue return 0, else return errno.
	 */
	int (*mio_iter_fini) (struct mtfs_io *io, int init_ret);
};

#endif /* __MTFS_IO_INTERNAL_H__ */
