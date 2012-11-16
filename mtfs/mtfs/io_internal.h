/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_IO_INTERNAL_H__
#define __MTFS_IO_INTERNAL_H__

#include <mtfs_io.h>
#include <mtfs_oplist.h>

int mtfs_io_init_oplist(struct mtfs_io *io, struct mtfs_oplist_object *oplist_obj);
void mtfs_io_iter_end_oplist(struct mtfs_io *io);
void mtfs_io_fini_oplist_noupdate(struct mtfs_io *io);
void mtfs_io_fini_oplist(struct mtfs_io *io);
void mtfs_io_fini_oplist_rename(struct mtfs_io *io);

void mtfs_io_iter_start_rw_nonoplist(struct mtfs_io *io);
int mtfs_io_iter_init_rw(struct mtfs_io *io);
void mtfs_io_iter_start_rw(struct mtfs_io *io);
void mtfs_io_iter_fini_read_ops(struct mtfs_io *io, int init_ret);
void mtfs_io_iter_fini_write_ops(struct mtfs_io *io, int init_ret);
void mtfs_io_iter_start_create(struct mtfs_io *io);
void mtfs_io_iter_start_link(struct mtfs_io *io);
void mtfs_io_iter_start_unlink(struct mtfs_io *io);
void mtfs_io_iter_start_mkdir(struct mtfs_io *io);
void mtfs_io_iter_start_rmdir(struct mtfs_io *io);
void mtfs_io_iter_start_mknod(struct mtfs_io *io);
void mtfs_io_iter_start_rename(struct mtfs_io *io);
void mtfs_io_iter_start_symlink(struct mtfs_io *io);
void mtfs_io_iter_start_readlink(struct mtfs_io *io);
void mtfs_io_iter_start_permission(struct mtfs_io *io);
void mtfs_io_iter_start_getattr(struct mtfs_io *io);
void mtfs_io_iter_start_setattr(struct mtfs_io *io);
void mtfs_io_iter_start_getxattr(struct mtfs_io *io);
void mtfs_io_iter_start_setxattr(struct mtfs_io *io);
void mtfs_io_iter_start_removexattr(struct mtfs_io *io);
void mtfs_io_iter_start_listxattr(struct mtfs_io *io);
void mtfs_io_iter_start_readdir(struct mtfs_io *io);
void mtfs_io_iter_start_open(struct mtfs_io *io);
void mtfs_io_iter_start_ioctl(struct mtfs_io *io);
void mtfs_io_iter_start_writepage(struct mtfs_io *io);
void mtfs_io_iter_start_readpage(struct mtfs_io *io);

#endif /* __MTFS_IO_INTERNAL_H__ */
