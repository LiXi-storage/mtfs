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
	MIOT_CREATE = 0,
	MIOT_LINK,
	MIOT_UNLINK,
	MIOT_MKDIR,
	MIOT_RMDIR,
	MIOT_MKNOD,
	MIOT_RENAME,
	MIOT_SYMLINK,
	MIOT_READLINK,
	MIOT_PERMISSION,
	MIOT_READV,
	MIOT_WRITEV,
	MIOT_GETATTR,
	MIOT_SETATTR,
	MIOT_GETXATTR,
	MIOT_SETXATTR,
	MIOT_REMOVEXATTR,
	MIOT_LISTXATTR,
	MIOT_READDIR,
	MIOT_OPEN,
	MIOT_IOCTL_WRITE,
	MIOT_IOCTL_READ,
	MIOT_WRITEPAGE,
	MIOT_READPAGE,
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
#include <mtfs_checksum.h>

struct mtfs_io_create {
	struct inode *dir;
	struct dentry *dentry;
	int mode;
	struct nameidata *nd;
};

struct mtfs_io_link {
	struct dentry *old_dentry;
	struct inode *dir;
	struct dentry *new_dentry;
};

struct mtfs_io_unlink {
	struct inode *dir;
	struct dentry *dentry;
};

struct mtfs_io_mkdir {
	struct inode *dir;
	struct dentry *dentry;
	int mode;
};

struct mtfs_io_rmdir {
	struct inode *dir;
	struct dentry *dentry;
};

struct mtfs_io_mknod {
	struct inode *dir;
	struct dentry *dentry;
	int mode;
	dev_t dev;
};

struct mtfs_io_rename {
	struct inode *old_dir;
	struct dentry *old_dentry;
	struct inode *new_dir;
	struct dentry *new_dentry;
};

struct mtfs_io_symlink {
	struct inode *dir;
	struct dentry *dentry;
	const char *symname;
};

struct mtfs_io_readlink {
	struct dentry *dentry;
	char __user *buf;
	int bufsiz;
};

struct mtfs_io_permission {
	struct inode *inode;
	int mask;
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

struct mtfs_io_readdir {
	struct file *file;
	void *dirent;
	filldir_t filldir;
};

struct mtfs_io_open {
	struct inode *inode;
	struct file *file;
};

struct mtfs_io_ioctl {
	struct inode *inode;
	struct file *file;
	unsigned int cmd;
	unsigned long arg;
	int is_kernel_ds;
};

struct mtfs_io_writepage {
	struct page *page;
	struct writeback_control *wbc;
};

struct mtfs_io_readpage {
	struct file *file;
	struct page *page;
};

struct mtfs_io_checksum_branch {
	int valid;
	ssize_t ssize;
	__u32 checksum; 
};

struct mtfs_io_checksum {
	struct mtfs_io_checksum_branch branch[MTFS_BRANCH_MAX]; /* Global bindex */
	struct mtfs_io_checksum_branch gather;                  /* First valid checksum */
	mchecksum_type_t               type;
	mtfs_list_t                    dirty_extents;           /* List of dirty extents */
};

struct mtfs_io {
	const struct mtfs_io_operations   *mi_ops;    /* Operations */
	mtfs_io_type_t                     mi_type;   /* IO type */
	mtfs_bindex_t                      mi_bindex; /* Branch index, not nessarily global */
	mtfs_bindex_t                      mi_bnum;   /* Branch number */
	mtfs_operation_result_t            mi_result; /* IO result */
	__u32                              mi_flags;  /* Result flags */
	int                                mi_break;  /* Continue to iter */

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
		struct mtfs_io_link        mi_link;
		struct mtfs_io_unlink      mi_unlink;
		struct mtfs_io_mkdir       mi_mkdir;
		struct mtfs_io_rmdir       mi_rmdir;
		struct mtfs_io_mknod       mi_mknod;
		struct mtfs_io_symlink     mi_symlink;
		struct mtfs_io_readlink    mi_readlink;
		struct mtfs_io_rw          mi_rw;
		struct mtfs_io_getattr     mi_getattr;
		struct mtfs_io_setattr     mi_setattr;
		struct mtfs_io_getxattr    mi_getxattr;
		struct mtfs_io_setxattr    mi_setxattr;
		struct mtfs_io_removexattr mi_removexattr;
		struct mtfs_io_listxattr   mi_listxattr;
		struct mtfs_io_rename      mi_rename;
		struct mtfs_io_permission  mi_permission;
		struct mtfs_io_readdir     mi_readdir;
		struct mtfs_io_open        mi_open;
		struct mtfs_io_ioctl       mi_ioctl;
		struct mtfs_io_writepage   mi_writepage;
		struct mtfs_io_readpage    mi_readpage;
	} u;
	union {
		struct mtfs_io_trace       mi_trace;
		struct mtfs_io_checksum    mi_checksum;
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

static inline mtfs_bindex_t mio_bindex(struct mtfs_io *io)
{
	if (io->mi_oplist.inited) {
		return io->mi_oplist.op_binfo[io->mi_bindex].bindex;
	}
	return io->mi_bindex;
}

extern int mtfs_io_loop(struct mtfs_io *io);
extern int mio_init_oplist(struct mtfs_io *io, struct mtfs_oplist_object *oplist_obj);
extern int mio_init_oplist_flag(struct mtfs_io *io);
extern void mio_iter_end_oplist(struct mtfs_io *io);
extern void mio_fini_oplist_noupdate(struct mtfs_io *io);
extern void mio_fini_oplist(struct mtfs_io *io);
extern void mio_fini_oplist_rename(struct mtfs_io *io);
extern void mio_unlock_mlock(struct mtfs_io *io);
extern int mio_lock_mlock(struct mtfs_io *io);

extern void mio_iter_start_rw_nonoplist(struct mtfs_io *io);
extern int mio_iter_init_rw(struct mtfs_io *io);
extern void mio_iter_start_rw(struct mtfs_io *io);
extern void mio_iter_fini_read_ops(struct mtfs_io *io, int init_ret);
extern void mio_iter_fini_write_ops(struct mtfs_io *io, int init_ret);
extern void mio_iter_start_create(struct mtfs_io *io);
extern void mio_iter_start_link(struct mtfs_io *io);
extern void mio_iter_start_unlink(struct mtfs_io *io);
extern void mio_iter_start_mkdir(struct mtfs_io *io);
extern void mio_iter_start_rmdir(struct mtfs_io *io);
extern void mio_iter_start_mknod(struct mtfs_io *io);
extern void mio_iter_start_rename(struct mtfs_io *io);
extern void mio_iter_start_symlink(struct mtfs_io *io);
extern void mio_iter_start_readlink(struct mtfs_io *io);
extern void mio_iter_start_permission(struct mtfs_io *io);
extern void mio_iter_start_getattr(struct mtfs_io *io);
extern void mio_iter_start_setattr(struct mtfs_io *io);
extern void mio_iter_start_getxattr(struct mtfs_io *io);
extern void mio_iter_start_setxattr(struct mtfs_io *io);
extern void mio_iter_start_removexattr(struct mtfs_io *io);
extern void mio_iter_start_listxattr(struct mtfs_io *io);
extern void mio_iter_start_readdir(struct mtfs_io *io);
extern void mio_iter_start_open(struct mtfs_io *io);
extern void mio_iter_start_ioctl(struct mtfs_io *io);
extern void mio_iter_start_writepage(struct mtfs_io *io);
extern void mio_iter_start_readpage(struct mtfs_io *io);
extern void mio_iter_end_readv(struct mtfs_io *io);

extern const struct mtfs_io_operations mtfs_io_ops[];
#endif /* defined (__linux__) && defined(__KERNEL__) */

#endif /* __MTFS_IO_H__ */
