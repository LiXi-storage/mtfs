/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <linux/module.h>
#include <mtfs_common.h>
#include <mtfs_device.h>
#include <mtfs_oplist.h>
#include <mtfs_checksum.h>
#include "file_internal.h"
#include "io_internal.h"
#include "inode_internal.h"
#include "ioctl_internal.h"
#include "mmap_internal.h"

int mio_init_oplist(struct mtfs_io *io, struct mtfs_oplist_object *oplist_obj)
{
	int ret = 0;
	struct dentry *dentry = NULL;
	struct inode *inode = NULL;
	struct mtfs_operation_list *oplist = &io->mi_oplist;
	MENTRY();

	dentry = io->mi_oplist_dentry;
	if (dentry) {
		inode = dentry->d_inode;
	} else {
		MASSERT(io->mi_oplist_inode);
		inode = io->mi_oplist_inode;
	}
	MASSERT(inode);

	mtfs_oplist_init(oplist, inode, oplist_obj);
	if (oplist->latest_bnum == 0) {
		if (dentry) {
			MERROR("[%.*s] has no valid branch, please check it\n",
			       dentry->d_name.len, dentry->d_name.name);
		} else {
			MERROR("file has no valid branch, please check it\n");
		}

		if (!mtfs_dev2noabort(mtfs_i2dev(inode))) {
			ret = -EIO;
		}
	}

	MRETURN(ret);
}
EXPORT_SYMBOL(mio_init_oplist);

int mio_init_oplist_flag(struct mtfs_io *io)
{
	return mio_init_oplist(io, &mtfs_oplist_flag);
}
EXPORT_SYMBOL(mio_init_oplist_flag);

void mio_iter_end_oplist(struct mtfs_io *io)
{
	struct mtfs_operation_list *oplist = &io->mi_oplist;
	MENTRY();

	mtfs_oplist_setbranch(oplist, io->mi_bindex,
	                      io->mi_flags,
	                      io->mi_result);

	_MRETURN();
}
EXPORT_SYMBOL(mio_iter_end_oplist);

void mio_fini_oplist_noupdate(struct mtfs_io *io)
{
	struct mtfs_operation_list *oplist = &io->mi_oplist;
	MENTRY();

	mtfs_oplist_gather(oplist);
	MASSERT(oplist->opinfo);
	MASSERT(oplist->opinfo->valid);
	io->mi_result = oplist->opinfo->result;
	io->mi_flags = oplist->opinfo->flags;

	_MRETURN();
}
EXPORT_SYMBOL(mio_fini_oplist_noupdate);

void mio_fini_oplist(struct mtfs_io *io)
{
	int ret = 0;
	struct dentry *dentry = NULL;
	struct inode *inode = NULL;
	struct mtfs_operation_list *oplist = &io->mi_oplist;
	MENTRY();

	mtfs_oplist_gather(oplist);
	MASSERT(oplist->opinfo);
	MASSERT(oplist->opinfo->valid);

	io->mi_result = oplist->opinfo->result;
	io->mi_flags = oplist->opinfo->flags;

	dentry = io->mi_oplist_dentry;
	if (dentry) {
		inode = dentry->d_inode;
	} else {
		MASSERT(io->mi_oplist_inode);
		inode = io->mi_oplist_inode;
	}
	MASSERT(inode);

	ret = mtfs_oplist_flush(oplist, inode);
	if (ret) {
		if (dentry) {
			MERROR("failed to flush oplist for [%.*s]\n",
			       dentry->d_name.len, dentry->d_name.name);
		} else {
			MERROR("failed to flush oplist\n");
		}	
		MBUG();
	}

	_MRETURN();
}
EXPORT_SYMBOL(mio_fini_oplist);

void mio_fini_oplist_rename(struct mtfs_io *io)
{
	int ret = 0;
	struct dentry *dentry = NULL;
	struct inode *inode = NULL;
	struct mtfs_operation_list *oplist = &io->mi_oplist;
	MENTRY();

	mtfs_oplist_gather(oplist);
	MASSERT(oplist->opinfo);
	MASSERT(oplist->opinfo->valid);
	io->mi_result = oplist->opinfo->result;
	io->mi_flags = oplist->opinfo->flags;

	dentry = io->mi_oplist_dentry;
	if (dentry) {
		inode = dentry->d_inode;
	} else {
		MASSERT(io->mi_oplist_inode);
		inode = io->mi_oplist_inode;
	}
	MASSERT(inode);

	ret = mtfs_oplist_flush(oplist, io->u.mi_rename.old_dir);
	if (ret) {
		MERROR("failed to update old dir\n");
		MBUG();
	}

	ret = mtfs_oplist_flush(oplist, io->u.mi_rename.new_dir);
	if (ret) {
		MERROR("failed to update new dir\n");
		MBUG();
	}

	_MRETURN();
}
EXPORT_SYMBOL(mio_fini_oplist_rename);

int mio_lock_mlock(struct mtfs_io *io)
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
EXPORT_SYMBOL(mio_lock_mlock);

void mio_unlock_mlock(struct mtfs_io *io)
{
	MENTRY();

	MASSERT(io->mi_mlock);
	mlock_cancel(io->mi_mlock);

	_MRETURN();
}
EXPORT_SYMBOL(mio_unlock_mlock);

int mio_iter_init_rw(struct mtfs_io *io)
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
EXPORT_SYMBOL(mio_iter_init_rw);

void mio_iter_start_rw_nonoplist(struct mtfs_io *io)
{
	struct mtfs_io_rw *io_rw = &io->u.mi_rw;
	int is_write = 0;
	MENTRY();

	is_write = (io->mi_type == MIOT_WRITEV) ? 1 : 0;
	io->mi_result.ssize = mtfs_file_rw_branch(is_write,
	                                          io_rw->file,
	                                          io_rw->iov,
	                                          io_rw->nr_segs,
	                                          io_rw->ppos,
	                                          io->mi_bindex);
	if (io->mi_result.ssize > 0) {
		/* TODO: this check is weak */
		io->mi_flags = MTFS_OPERATION_SUCCESS | MTFS_OPERATION_PREFERABLE;
	} else {
		io->mi_flags = 0;
	}

	_MRETURN();
}
EXPORT_SYMBOL(mio_iter_start_rw_nonoplist);

void mio_iter_fini_read_ops(struct mtfs_io *io, int init_ret)
{
	struct inode *inode = NULL;
	MENTRY();

	inode = io->mi_oplist_dentry != NULL ?
	        io->mi_oplist_dentry->d_inode :
	        io->mi_oplist_inode;

	if (unlikely(init_ret)) {
		if (io->mi_bindex == io->mi_oplist.latest_bnum - 1) {
			io->mi_break = 1;
		} else {
			io->mi_bindex++;
		}
		MBUG();
		goto out;
	}

	if ((io->mi_flags & MTFS_OPERATION_SUCCESS) &&
	    ((io->mi_type != MIOT_READV) ||
	     (!mtfs_dev2checksum(mtfs_i2dev(inode))))) {
		io->mi_break = 1;
		goto out;
	}

	if (io->mi_bindex == io->mi_oplist.latest_bnum - 1) {
		if (!mtfs_dev2noabort(mtfs_i2dev(inode))) {
			io->mi_break = 1;
			goto out;
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
EXPORT_SYMBOL(mio_iter_fini_read_ops);

void mio_iter_fini_write_ops(struct mtfs_io *io, int init_ret)
{
	MENTRY();

	if (unlikely(init_ret)) {
		if (io->mi_bindex == io->mi_bnum - 1) {
			io->mi_break = 1;
		} else {
			io->mi_bindex++;
		}
		MBUG();
		goto out;
	}

	if (io->mi_bindex == io->mi_oplist.latest_bnum - 1) {
		if (io->mi_oplist.success_latest_bnum <= 0) {
			MDEBUG("operation failed for all latest %d branches\n",
			       io->mi_oplist.latest_bnum);
			if (io->mi_oplist_dentry) {
				if (!mtfs_dev2noabort(mtfs_i2dev(io->mi_oplist_dentry->d_inode))) {
					io->mi_break = 1;
					goto out;
				}
			} else {
				MASSERT(io->mi_oplist_inode);
				if (!mtfs_dev2noabort(mtfs_i2dev(io->mi_oplist_inode))) {
					io->mi_break = 1;
					goto out;
				}
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
EXPORT_SYMBOL(mio_iter_fini_write_ops);

void mio_iter_start_rw(struct mtfs_io *io)
{
	struct mtfs_io_rw *io_rw = &io->u.mi_rw;
	mtfs_bindex_t global_bindex = mio_bindex(io);
	int is_write = 0;
	MENTRY();

	is_write = (io->mi_type == MIOT_WRITEV) ? 1 : 0;
	io->mi_result.ssize = mtfs_file_rw_branch(is_write,
	                                          io_rw->file,
	                                          io_rw->iov_tmp,
	                                          io_rw->nr_segs,
	                                          &io_rw->pos_tmp,
	                                          global_bindex);
	if (io->mi_result.ssize >= 0) {
		io->mi_flags = MTFS_OPERATION_SUCCESS;
		if (io->mi_result.ssize == io_rw->rw_size) {
			io->mi_flags |= MTFS_OPERATION_PREFERABLE;
		}
	} else {
		io->mi_flags = 0;
	}

	_MRETURN();
}
EXPORT_SYMBOL(mio_iter_start_rw);

void mio_iter_start_create(struct mtfs_io *io)
{
	struct mtfs_io_create *io_create = &io->u.mi_create;
	mtfs_bindex_t global_bindex = mio_bindex(io);
	MENTRY();

	io->mi_result.ret = mtfs_create_branch(io_create->dir,
	                                       io_create->dentry,
	                                       io_create->mode,
	                                       io_create->nd,
	                                       global_bindex);

	if (!io->mi_result.ret) {
		io->mi_flags = MTFS_OPERATION_SUCCESS | MTFS_OPERATION_PREFERABLE;
	} else {
		io->mi_flags = 0;
	}

	_MRETURN();
}
EXPORT_SYMBOL(mio_iter_start_create);

void mio_iter_start_link(struct mtfs_io *io)
{
	struct mtfs_io_link *io_link = &io->u.mi_link;
	mtfs_bindex_t global_bindex = mio_bindex(io);
	MENTRY();

	io->mi_result.ret = mtfs_link_branch(io_link->old_dentry,
	                                     io_link->dir,
	                                     io_link->new_dentry,
	                                     global_bindex);

	if (!io->mi_result.ret) {
		io->mi_flags = MTFS_OPERATION_SUCCESS | MTFS_OPERATION_PREFERABLE;
	} else {
		io->mi_flags = 0;
	}

	_MRETURN();
}
EXPORT_SYMBOL(mio_iter_start_link);

void mio_iter_start_unlink(struct mtfs_io *io)
{
	struct mtfs_io_unlink *io_unlink = &io->u.mi_unlink;
	mtfs_bindex_t global_bindex = mio_bindex(io);
	MENTRY();

	io->mi_result.ret = mtfs_unlink_branch(io_unlink->dir,
	                                       io_unlink->dentry,
	                                       global_bindex);

	if (!io->mi_result.ret) {
		io->mi_flags = MTFS_OPERATION_SUCCESS | MTFS_OPERATION_PREFERABLE;
	} else if (io->mi_result.ret == -ENOENT) {
		io->mi_flags = MTFS_OPERATION_SUCCESS;
	} else {
		io->mi_flags = 0;
	}

	_MRETURN();
}
EXPORT_SYMBOL(mio_iter_start_unlink);

void mio_iter_start_mkdir(struct mtfs_io *io)
{
	struct mtfs_io_mkdir *io_mkdir = &io->u.mi_mkdir;
	mtfs_bindex_t global_bindex = mio_bindex(io);
	MENTRY();

	io->mi_result.ret = mtfs_mkdir_branch(io_mkdir->dir,
	                                      io_mkdir->dentry,
	                                      io_mkdir->mode,
	                                      global_bindex);

	if (!io->mi_result.ret) {
		io->mi_flags = MTFS_OPERATION_SUCCESS | MTFS_OPERATION_PREFERABLE;
	} else {
		io->mi_flags = 0;
	}

	_MRETURN();
}
EXPORT_SYMBOL(mio_iter_start_mkdir);

void mio_iter_start_rmdir(struct mtfs_io *io)
{
	struct mtfs_io_rmdir *io_rmdir = &io->u.mi_rmdir;
	mtfs_bindex_t global_bindex = mio_bindex(io);
	MENTRY();

	io->mi_result.ret = mtfs_rmdir_branch(io_rmdir->dir,
	                                      io_rmdir->dentry,
	                                      global_bindex);

	if (!io->mi_result.ret) {
		io->mi_flags = MTFS_OPERATION_SUCCESS | MTFS_OPERATION_PREFERABLE;
	} else if (io->mi_result.ret == -ENOENT) {
		io->mi_flags = MTFS_OPERATION_SUCCESS;
	} else {
		io->mi_flags = 0;
	}

	_MRETURN();
}
EXPORT_SYMBOL(mio_iter_start_rmdir);

void mio_iter_start_mknod(struct mtfs_io *io)
{
	struct mtfs_io_mknod *io_mknod = &io->u.mi_mknod;
	mtfs_bindex_t global_bindex = mio_bindex(io);
	MENTRY();

	io->mi_result.ret = mtfs_mknod_branch(io_mknod->dir,
	                                      io_mknod->dentry,
	                                      io_mknod->mode,
	                                      io_mknod->dev,
	                                      global_bindex);

	if (!io->mi_result.ret) {
		io->mi_flags = MTFS_OPERATION_SUCCESS | MTFS_OPERATION_PREFERABLE;
	} else {
		io->mi_flags = 0;
	}

	_MRETURN();
}
EXPORT_SYMBOL(mio_iter_start_mknod);

void mio_iter_start_rename(struct mtfs_io *io)
{
	struct mtfs_io_rename *io_rename = &io->u.mi_rename;
	mtfs_bindex_t global_bindex = mio_bindex(io);
	MENTRY();

	io->mi_result.ret = mtfs_rename_branch(io_rename->old_dir,
	                                       io_rename->old_dentry,
	                                       io_rename->new_dir,
	                                       io_rename->new_dentry,
	                                       global_bindex);

	if (!io->mi_result.ret) {
		io->mi_flags = MTFS_OPERATION_SUCCESS | MTFS_OPERATION_PREFERABLE;
	} else {
		io->mi_flags = 0;
	}

	_MRETURN();
}
EXPORT_SYMBOL(mio_iter_start_rename);

void mio_iter_start_symlink(struct mtfs_io *io)
{
	struct mtfs_io_symlink *io_symlink = &io->u.mi_symlink;
	mtfs_bindex_t global_bindex = mio_bindex(io);
	MENTRY();

	io->mi_result.ret = mtfs_symlink_branch(io_symlink->dir,
	                                        io_symlink->dentry,
	                                        io_symlink->symname,
	                                        global_bindex);

	if (!io->mi_result.ret) {
		io->mi_flags = MTFS_OPERATION_SUCCESS | MTFS_OPERATION_PREFERABLE;
	} else {
		io->mi_flags = 0;
	}

	_MRETURN();
}
EXPORT_SYMBOL(mio_iter_start_symlink);

void mio_iter_start_readlink(struct mtfs_io *io)
{
	struct mtfs_io_readlink *io_readlink = &io->u.mi_readlink;
	mtfs_bindex_t global_bindex = mio_bindex(io);
	MENTRY();

	io->mi_result.ret = mtfs_readlink_branch(io_readlink->dentry,
	                                         io_readlink->buf,
	                                         io_readlink->bufsiz,
	                                         global_bindex);

	if (io->mi_result.ret >= 0) {
		io->mi_flags = MTFS_OPERATION_SUCCESS | MTFS_OPERATION_PREFERABLE;
	} else {
		io->mi_flags = 0;
	}

	_MRETURN();
}
EXPORT_SYMBOL(mio_iter_start_readlink);

void mio_iter_start_permission(struct mtfs_io *io)
{
	struct mtfs_io_permission *io_permission = &io->u.mi_permission;
	mtfs_bindex_t global_bindex = mio_bindex(io);
	MENTRY();

#ifdef HAVE_INODE_PERMISION_2ARGS
	io->mi_result.ret = mtfs_permission_branch(io_permission->inode,
	                                           io_permission->mask,
	                                           global_bindex);
#else /* HAVE_INODE_PERMISION_2ARGS */
	io->mi_result.ret = mtfs_permission_branch(io_permission->inode,
	                                           io_permission->mask,
	                                           io_permission->nd,
	                                           global_bindex);
#endif /* HAVE_INODE_PERMISION_2ARGS */

	io->mi_flags = MTFS_OPERATION_SUCCESS | MTFS_OPERATION_PREFERABLE;
	_MRETURN();
}
EXPORT_SYMBOL(mio_iter_start_permission);

void mio_iter_start_getattr(struct mtfs_io *io)
{
	struct mtfs_io_getattr *io_getattr = &io->u.mi_getattr;
	mtfs_bindex_t global_bindex = mio_bindex(io);
	MENTRY();

	io->mi_result.ret = mtfs_getattr_branch(io_getattr->mnt,
	                                        io_getattr->dentry,
	                                        io_getattr->stat,
	                                        global_bindex);
	if (!io->mi_result.ret) {
		io->mi_flags = MTFS_OPERATION_SUCCESS | MTFS_OPERATION_PREFERABLE;
	} else {
		io->mi_flags = 0;
	}

	_MRETURN();
}
EXPORT_SYMBOL(mio_iter_start_getattr);

void mio_iter_start_setattr(struct mtfs_io *io)
{
	struct mtfs_io_setattr *io_setattr = &io->u.mi_setattr;
	mtfs_bindex_t global_bindex = mio_bindex(io);
	MENTRY();

	io->mi_result.ret = mtfs_setattr_branch(io_setattr->dentry,
	                                        io_setattr->ia,
	                                        global_bindex);
	if (!io->mi_result.ret) {
		io->mi_flags = MTFS_OPERATION_SUCCESS | MTFS_OPERATION_PREFERABLE;
	} else {
		io->mi_flags = 0;
	}

	_MRETURN();
}
EXPORT_SYMBOL(mio_iter_start_setattr);

void mio_iter_start_getxattr(struct mtfs_io *io)
{
	struct mtfs_io_getxattr *io_getxattr = &io->u.mi_getxattr;
	mtfs_bindex_t global_bindex = mio_bindex(io);
	MENTRY();

	io->mi_result.ssize = mtfs_getxattr_branch(io_getxattr->dentry,
	                                           io_getxattr->name,
	                                           io_getxattr->value,
	                                           io_getxattr->size,
	                                           global_bindex);
	if (io->mi_result.ssize >= 0) {
		if (io_getxattr->size > 0) {
			MASSERT(io->mi_result.ssize <= io_getxattr->size);
		}
		io->mi_flags = MTFS_OPERATION_SUCCESS | MTFS_OPERATION_PREFERABLE;
	} else {
		io->mi_flags = 0;
	}

	_MRETURN();
}
EXPORT_SYMBOL(mio_iter_start_getxattr);

void mio_iter_start_setxattr(struct mtfs_io *io)
{
	struct mtfs_io_setxattr *io_setxattr = &io->u.mi_setxattr;
	mtfs_bindex_t global_bindex = mio_bindex(io);
	MENTRY();

	io->mi_result.ret = mtfs_setxattr_branch(io_setxattr->dentry,
	                                         io_setxattr->name,
	                                         io_setxattr->value,
	                                         io_setxattr->size,
	                                         io_setxattr->flags,
	                                         global_bindex);
	if (!io->mi_result.ret) {
		io->mi_flags = MTFS_OPERATION_SUCCESS | MTFS_OPERATION_PREFERABLE;
	} else {
		io->mi_flags = 0;
	}

	_MRETURN();
}
EXPORT_SYMBOL(mio_iter_start_setxattr);

void mio_iter_start_removexattr(struct mtfs_io *io)
{
	struct mtfs_io_removexattr *io_removexattr = &io->u.mi_removexattr;
	mtfs_bindex_t global_bindex = mio_bindex(io);
	MENTRY();

	io->mi_result.ret = mtfs_removexattr_branch(io_removexattr->dentry,
	                                            io_removexattr->name,
	                                            global_bindex);
	if (!io->mi_result.ret) {
		io->mi_flags = MTFS_OPERATION_SUCCESS | MTFS_OPERATION_PREFERABLE;
	} else {
		io->mi_flags = 0;
	}

	_MRETURN();
}
EXPORT_SYMBOL(mio_iter_start_removexattr);

void mio_iter_start_listxattr(struct mtfs_io *io)
{
	struct mtfs_io_listxattr *io_listxattr = &io->u.mi_listxattr;
	mtfs_bindex_t global_bindex = mio_bindex(io);
	MENTRY();

	io->mi_result.ssize = mtfs_listxattr_branch(io_listxattr->dentry,
	                                            io_listxattr->list,
	                                            io_listxattr->size,
	                                            global_bindex);
	if (io->mi_result.ssize >= 0) {
		if (io_listxattr->size > 0) {
			MASSERT(io->mi_result.ssize <= io_listxattr->size);
		}
		io->mi_flags = MTFS_OPERATION_SUCCESS | MTFS_OPERATION_PREFERABLE;
	} else {
		io->mi_flags = 0;
	}

	_MRETURN();
}
EXPORT_SYMBOL(mio_iter_start_listxattr);

void mio_iter_start_readdir(struct mtfs_io *io)
{
	struct mtfs_io_readdir *io_readdir = &io->u.mi_readdir;
	mtfs_bindex_t global_bindex = mio_bindex(io);
	MENTRY();

	io->mi_result.ret = mtfs_readdir_branch(io_readdir->file,
	                                        io_readdir->dirent,
	                                        io_readdir->filldir,
	                                        global_bindex);
	if (!io->mi_result.ret) {
		io->mi_flags = MTFS_OPERATION_SUCCESS | MTFS_OPERATION_PREFERABLE;
	} else {
		io->mi_flags = 0;
	}

	_MRETURN();
}
EXPORT_SYMBOL(mio_iter_start_readdir);

void mio_iter_start_open(struct mtfs_io *io)
{
	struct mtfs_io_open *io_open = &io->u.mi_open;
	mtfs_bindex_t global_bindex = mio_bindex(io);
	MENTRY();

	io->mi_result.ret = mtfs_open_branch(io_open->inode,
	                                     io_open->file,
	                                     global_bindex);
	if (!io->mi_result.ret) {
		io->mi_flags = MTFS_OPERATION_SUCCESS | MTFS_OPERATION_PREFERABLE;
	} else {
		io->mi_flags = 0;
	}

	_MRETURN();
}
EXPORT_SYMBOL(mio_iter_start_open);

void mio_iter_start_ioctl(struct mtfs_io *io)
{
	struct mtfs_io_ioctl *io_ioctl = &io->u.mi_ioctl;
	mtfs_bindex_t global_bindex = mio_bindex(io);
	MENTRY();

	io->mi_result.ret = mtfs_ioctl_branch(io_ioctl->inode,
	                                      io_ioctl->file,
	                                      io_ioctl->cmd,
	                                      io_ioctl->arg,
	                                      global_bindex,
	                                      io_ioctl->is_kernel_ds);
	if (!io->mi_result.ret) {
		io->mi_flags = MTFS_OPERATION_SUCCESS | MTFS_OPERATION_PREFERABLE;
	} else {
		io->mi_flags = 0;
	}

	_MRETURN();
}
EXPORT_SYMBOL(mio_iter_start_ioctl);

void mio_iter_start_writepage(struct mtfs_io *io)
{
	struct mtfs_io_writepage *io_writepage = &io->u.mi_writepage;
	mtfs_bindex_t global_bindex = mio_bindex(io);
	MENTRY();

	io->mi_result.ret = mtfs_writepage_branch(io_writepage->page,
	                                          io_writepage->wbc,
	                                          global_bindex);
	if (!io->mi_result.ret) {
		io->mi_flags = MTFS_OPERATION_SUCCESS | MTFS_OPERATION_PREFERABLE;
	} else {
		io->mi_flags = 0;
	}

	_MRETURN();
}
EXPORT_SYMBOL(mio_iter_start_writepage);

void mio_iter_start_readpage(struct mtfs_io *io)
{
	struct mtfs_io_readpage *io_readpage = &io->u.mi_readpage;
	mtfs_bindex_t global_bindex = mio_bindex(io);
	MENTRY();

	io->mi_result.ret = mtfs_readpage_branch(io_readpage->file,
	                                         io_readpage->page,
	                                         global_bindex);
	if (!io->mi_result.ret) {
		io->mi_flags = MTFS_OPERATION_SUCCESS | MTFS_OPERATION_PREFERABLE;
	} else {
		io->mi_flags = 0;
	}

	_MRETURN();
}
EXPORT_SYMBOL(mio_iter_start_readpage);

__u32 mio_iov_chechsum(const struct iovec *iov,
                     unsigned long nr_segs,
                     size_t size,
                     mchecksum_type_t type)
{
	__u32 tmp_checksum = 0;
	unsigned long tmp_seg = 0;
	size_t tmp_total = 0;
	size_t tmp_len = 0;
	const struct iovec *tmp_iov = NULL;
	MENTRY();

	tmp_checksum = mchecksum_init(type);
	for (tmp_seg = 0; tmp_seg < nr_segs; tmp_seg++) {
		tmp_iov = &iov[tmp_seg];
		MASSERT(tmp_iov->iov_len >= 0);
		if (tmp_total + tmp_iov->iov_len > size) {
			tmp_len = size - tmp_total;
		} else {
			tmp_len = tmp_iov->iov_len;
		}

		if (tmp_len > 0) {
			tmp_checksum = mchecksum_compute(tmp_checksum,
			                                 tmp_iov->iov_base,
			                                 tmp_len,
			                                 type);
		}
 		tmp_total += tmp_iov->iov_len;
 		if (tmp_total >= size) {
 			break;
 		}
	}

	MRETURN(tmp_checksum);
}

static void mio_iter_check_readv(struct mtfs_io *io)
{
	struct mtfs_io_rw *io_rw = &io->u.mi_rw;
	struct file *file = io_rw->file;
	mtfs_bindex_t global_bindex = mio_bindex(io);
	struct mtfs_io_checksum *checksum = &io->subject.mi_checksum;
	__u32 tmp_checksum = 0;
	MENTRY();

	if ((io->mi_flags & MTFS_OPERATION_SUCCESS) &&
	    mtfs_dev2checksum(mtfs_f2dev(file))) {
		if (checksum->gather.valid) {
			if (checksum->gather.ssize != io->mi_result.ssize) {
				MERROR("read size of branch[%d] of file [%.*s] is different, "
				       "expected %lld, got %lld\n",
				       global_bindex,
				       file->f_dentry->d_name.len,
				       file->f_dentry->d_name.name,
				       checksum->gather.ssize,
				       io->mi_result.ssize);
				if (strcmp(mtfs_dev2junction(mtfs_f2dev(file))->mj_subject,
				           "ASYNC_REPLICA") == 0) {
					_MRETURN();
				} else {
					/* TODO: handle write size for async replication*/
					MBUG();
				}
			}
		}

		tmp_checksum = mio_iov_chechsum(io_rw->iov,
		                                io_rw->nr_segs,
		                                io->mi_result.ssize,
		                                checksum->type);
		if (checksum->gather.valid) {
			if (checksum->gather.checksum != tmp_checksum) {
				MERROR("content of branch[%d] of file [%.*s] is different, "
				       "expected 0x%x, got 0x%x\n",
				       global_bindex,
				       file->f_dentry->d_name.len,
				       file->f_dentry->d_name.name,
				       checksum->gather.checksum,
				       tmp_checksum);
				MBUG();
			}
		} else {
			checksum->gather.valid = 1;
			checksum->gather.ssize = io->mi_result.ssize;
			checksum->gather.checksum = tmp_checksum;
		}
		checksum->branch[global_bindex].valid = 1;
		checksum->branch[global_bindex].ssize = io->mi_result.ssize;
		checksum->branch[global_bindex].checksum = tmp_checksum;
	}

	_MRETURN();
}

void mio_iter_end_readv(struct mtfs_io *io)
{
	mio_iter_end_oplist(io);
	mio_iter_check_readv(io);
}
EXPORT_SYMBOL(mio_iter_end_readv);

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
