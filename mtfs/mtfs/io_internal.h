/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_IO_INTERNAL_H__
#define __MTFS_IO_INTERNAL_H__

#include <mtfs_io.h>

extern const struct mtfs_io_operations mtfs_io_ops[];

void mtfs_io_iter_start_rw_nonoplist(struct mtfs_io *io);
int mtfs_io_iter_init_rw(struct mtfs_io *io);

#endif /* __MTFS_IO_INTERNAL_H__ */
