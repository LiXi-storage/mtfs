/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_IO_H__
#define __MTFS_IO_H__

#include <mtfs_oplist.h>
#include <mtfs_record.h>
#if defined(__linux__) && defined(__KERNEL__)
#include <linux/time.h>
#else
#include <sys/time.h>
#endif

typedef enum mtfs_io_type {
	MIT_CREATE,
	MIT_READV,
	MIT_WRITEV,
	MIT_GETATTR,
	MIT_SETATTR,
	MIT_GETXATTR,
	MIT_SETXATTR,
	MIT_REMOVEXATTR,
	MIT_LISTXATTR,
} mtfs_io_type_t;

struct mtfs_io_trace {
	struct mrecord_head     head;
	mtfs_io_type_t          type;
	mtfs_operation_result_t result;
	struct timeval          start;
	struct timeval          end;
};


#if defined(__linux__) && defined(__KERNEL__)

#include <linux/fs.h>
#include <linux/uio.h>
#include <mtfs_lock.h>
#include <mtfs_trace.h>

struct mtfs_io_create {
	struct inode *dir;
	struct dentry *dentry;
	int mode;
	struct nameidata *nd;
};

struct mtfs_io_rw {
	struct file *file;
	const struct iovec *iov;
	unsigned long nr_segs;
	loff_t *ppos;
	size_t rw_size;
	ssize_t ret;

	struct iovec *iov_tmp;
	loff_t pos_tmp;
	size_t iov_length;
};

struct mtfs_io_getattr {
	struct vfsmount *mnt;
	struct dentry *dentry;
	struct kstat *stat;
};

struct mtfs_io_setattr {
	struct dentry *dentry;
	struct iattr *ia;
};

struct mtfs_io_getxattr {
	struct dentry *dentry;
	const char *name;
	void *value;
	size_t size;
};

struct mtfs_io_setxattr {
	struct dentry *dentry;
	const char *name;
	const void *value;
	size_t size;
	int flags;
};

struct mtfs_io_removexattr {
	struct dentry *dentry;
	const char *name;
};

struct mtfs_io_listxattr {
	struct dentry *dentry;
	char *list;
	size_t size;
};

struct mtfs_io {
	const struct mtfs_io_operations   *mi_ops;
	mtfs_io_type_t                     mi_type;
	mtfs_bindex_t                      mi_bindex;
	mtfs_bindex_t                      mi_bnum;
	mtfs_operation_result_t            mi_result;
	int                                mi_successful;
	int                                mi_break;

	/*
	 * Inited when ->mio_init
	 * Set when ->mio_iter_end
	 * Checked when ->mio_fini
	 */
	struct mtfs_operation_list         mi_oplist;
	struct dentry                     *mi_oplist_dentry;
	/* mi_oplist_dentry may not be available */
	struct inode                      *mi_oplist_inode;
	/*
	 * Locked when ->mio_lock
	 * Unlocked when ->mio_unlock
	 * TODO: alloc with io
	 */
	struct mlock                      *mi_mlock;
	struct mlock_resource             *mi_resource;
	struct mlock_enqueue_info          mi_einfo;

	union {
		struct mtfs_io_create      mi_create;
		struct mtfs_io_rw          mi_rw;
		struct mtfs_io_getattr     mi_getattr;
		struct mtfs_io_setattr     mi_setattr;
		struct mtfs_io_getxattr    mi_getxattr;
		struct mtfs_io_setxattr    mi_setxattr;
		struct mtfs_io_removexattr mi_removexattr;
		struct mtfs_io_listxattr   mi_listxattr;
	} u;
	union {
		struct mtfs_io_trace     mi_trace;
	} subject;
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
	void (*mio_iter_fini) (struct mtfs_io *io, int init_ret);
};

extern int mtfs_io_loop(struct mtfs_io *io);
#endif /* defined (__linux__) && defined(__KERNEL__) */

#endif /* __MTFS_IO_H__ */
