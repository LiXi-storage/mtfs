/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <linux/module.h>
#include <mtfs_io.h>
#include <mtfs_device.h>
#include <mtfs_inode.h>

static int msync_io_lock_setattr(struct mtfs_io *io)
{
	int ret = 0;
	struct mtfs_io_setattr *io_setattr = &io->u.mi_setattr;
	struct iattr *attr = io_setattr->ia;
	int ia_valid = attr->ia_valid;
	MENTRY();

	if (ia_valid & ATTR_SIZE) {
		ret = mio_lock_mlock(io);
	}

	MRETURN(ret);
}

static void msync_io_unlock_setattr(struct mtfs_io *io)
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

static int msync_io_init_readv(struct mtfs_io *io)
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

	ret = mio_init_oplist_flag(io);
	if (!ret && mtfs_dev2checksum(mtfs_i2dev(inode))) {
		io->subject.mi_checksum.type = mchecksum_type_select();	
	}

	MRETURN(ret);
}

const struct mtfs_io_operations mtfs_io_ops[] = {
	[MIOT_CREATE] = {
		.mio_init       = mio_init_oplist_flag,
		.mio_fini       = mio_fini_oplist,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mio_iter_start_create,
		.mio_iter_end   = mio_iter_end_oplist,
		.mio_iter_fini  = mio_iter_fini_write_ops,
	},
	[MIOT_LINK] = {
		.mio_init       = mio_init_oplist_flag,
		.mio_fini       = mio_fini_oplist,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mio_iter_start_link,
		.mio_iter_end   = mio_iter_end_oplist,
		.mio_iter_fini  = mio_iter_fini_write_ops,
	},
	[MIOT_UNLINK] = {
		.mio_init       = mio_init_oplist_flag,
		.mio_fini       = mio_fini_oplist,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mio_iter_start_unlink,
		.mio_iter_end   = mio_iter_end_oplist,
		.mio_iter_fini  = mio_iter_fini_write_ops,
	},
	[MIOT_MKDIR] = {
		.mio_init       = mio_init_oplist_flag,
		.mio_fini       = mio_fini_oplist,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mio_iter_start_mkdir,
		.mio_iter_end   = mio_iter_end_oplist,
		.mio_iter_fini  = mio_iter_fini_write_ops,
	},
	[MIOT_RMDIR] = {
		.mio_init       = mio_init_oplist_flag,
		.mio_fini       = mio_fini_oplist,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mio_iter_start_rmdir,
		.mio_iter_end   = mio_iter_end_oplist,
		.mio_iter_fini  = mio_iter_fini_write_ops,
	},
	[MIOT_MKNOD] = {
		.mio_init       = mio_init_oplist_flag,
		.mio_fini       = mio_fini_oplist,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mio_iter_start_mknod,
		.mio_iter_end   = mio_iter_end_oplist,
		.mio_iter_fini  = mio_iter_fini_write_ops,
	},
	[MIOT_RENAME] = {
		.mio_init       = mio_init_oplist_flag,
		.mio_fini       = mio_fini_oplist_rename,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mio_iter_start_rename,
		.mio_iter_end   = mio_iter_end_oplist,
		.mio_iter_fini  = mio_iter_fini_write_ops,
	},
	[MIOT_SYMLINK] = {
		.mio_init       = mio_init_oplist_flag,
		.mio_fini       = mio_fini_oplist,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mio_iter_start_symlink,
		.mio_iter_end   = mio_iter_end_oplist,
		.mio_iter_fini  = mio_iter_fini_write_ops,
	},
	[MIOT_READLINK] = {
		.mio_init       = mio_init_oplist_flag,
		.mio_fini       = NULL,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mio_iter_start_readlink,
		.mio_iter_end   = NULL,
		.mio_iter_fini  = mio_iter_fini_read_ops,
	},
	[MIOT_PERMISSION] = {
		.mio_init       = mio_init_oplist_flag,
		.mio_fini       = NULL,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mio_iter_start_permission,
		.mio_iter_end   = NULL,
		.mio_iter_fini  = mio_iter_fini_read_ops,
	},
	[MIOT_READV] = {
		.mio_init       = msync_io_init_readv,
		.mio_fini       = mio_fini_oplist_noupdate,
		.mio_lock       = mio_lock_mlock,
		.mio_unlock     = mio_unlock_mlock,
		.mio_iter_init  = mio_iter_init_rw,
		.mio_iter_start = mio_iter_start_rw,
		.mio_iter_end   = mio_iter_end_readv,
		.mio_iter_fini  = mio_iter_fini_read_ops,
	},
	[MIOT_WRITEV] = {
		.mio_init       = mio_init_oplist_flag,
		.mio_fini       = mio_fini_oplist,
		.mio_lock       = mio_lock_mlock,
		.mio_unlock     = mio_unlock_mlock,
		.mio_iter_init  = mio_iter_init_rw,
		.mio_iter_start = mio_iter_start_rw,
		.mio_iter_end   = mio_iter_end_oplist,
		.mio_iter_fini  = mio_iter_fini_write_ops,
	},
	[MIOT_GETATTR] = {
		.mio_init       = mio_init_oplist_flag,
		.mio_fini       = NULL,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mio_iter_start_getattr,
		.mio_iter_end   = NULL,
		.mio_iter_fini  = mio_iter_fini_read_ops,
	},
	[MIOT_SETATTR] = {
		.mio_init       = mio_init_oplist_flag,
		.mio_fini       = mio_fini_oplist,
		.mio_lock       = msync_io_lock_setattr,
		.mio_unlock     = msync_io_unlock_setattr,
		.mio_iter_init  = NULL,
		.mio_iter_start = mio_iter_start_setattr,
		.mio_iter_end   = mio_iter_end_oplist,
		.mio_iter_fini  = mio_iter_fini_write_ops,
	},
	[MIOT_GETXATTR] = {
		.mio_init       = mio_init_oplist_flag,
		.mio_fini       = NULL,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mio_iter_start_getxattr,
		.mio_iter_end   = NULL,
		.mio_iter_fini  = mio_iter_fini_read_ops,
	},
	[MIOT_SETXATTR] = {
		.mio_init       = mio_init_oplist_flag,
		.mio_fini       = mio_fini_oplist,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mio_iter_start_setxattr,
		.mio_iter_end   = mio_iter_end_oplist,
		.mio_iter_fini  = mio_iter_fini_write_ops,
	},
	[MIOT_REMOVEXATTR] = {
		.mio_init       = mio_init_oplist_flag,
		.mio_fini       = mio_fini_oplist,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mio_iter_start_removexattr,
		.mio_iter_end   = mio_iter_end_oplist,
		.mio_iter_fini  = mio_iter_fini_write_ops,
	},
	[MIOT_LISTXATTR] = {
		.mio_init       = mio_init_oplist_flag,
		.mio_fini       = NULL,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mio_iter_start_listxattr,
		.mio_iter_end   = NULL,
		.mio_iter_fini  = mio_iter_fini_read_ops,
	},
	[MIOT_READDIR] = {
		.mio_init       = mio_init_oplist_flag,
		.mio_fini       = NULL,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mio_iter_start_readdir,
		.mio_iter_end   = NULL,
		.mio_iter_fini  = mio_iter_fini_read_ops,
	},
	[MIOT_OPEN] = {
		.mio_init       = mio_init_oplist_flag,
		.mio_fini       = mio_fini_oplist_noupdate,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mio_iter_start_open,
		.mio_iter_end   = mio_iter_end_oplist,
		.mio_iter_fini  = mio_iter_fini_write_ops,
	},
	[MIOT_IOCTL_WRITE] = {
		.mio_init       = mio_init_oplist_flag,
		.mio_fini       = mio_fini_oplist,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mio_iter_start_ioctl,
		.mio_iter_end   = mio_iter_end_oplist,
		.mio_iter_fini  = mio_iter_fini_write_ops,
	},
	[MIOT_IOCTL_READ] = {
		.mio_init       = mio_init_oplist_flag,
		.mio_fini       = NULL,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mio_iter_start_ioctl,
		.mio_iter_end   = NULL,
		.mio_iter_fini  = mio_iter_fini_read_ops,
	},
	[MIOT_WRITEPAGE] = {
		.mio_init       = mio_init_oplist_flag,
		.mio_fini       = mio_fini_oplist,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mio_iter_start_writepage,
		.mio_iter_end   = mio_iter_end_oplist,
		.mio_iter_fini  = mio_iter_fini_write_ops,
	},
	[MIOT_READPAGE] = {
		.mio_init       = mio_init_oplist_flag,
		.mio_fini       = NULL,
		.mio_lock       = NULL,
		.mio_unlock     = NULL,
		.mio_iter_init  = NULL,
		.mio_iter_start = mio_iter_start_readpage,
		.mio_iter_end   = NULL,
		.mio_iter_fini  = mio_iter_fini_read_ops,
	},
};
EXPORT_SYMBOL(mtfs_io_ops);

static int __init msync_init(void)
{
	int ret = 0;
	MENTRY();

	MRETURN(ret);
}

static void __exit msync_exit(void)
{
	MENTRY();

	_MRETURN();
}


MODULE_AUTHOR("MulTi File System Development Workgroup");
MODULE_DESCRIPTION("sync replica subject for mtfs");
MODULE_LICENSE("GPL");

module_init(msync_init)
module_exit(msync_exit)
