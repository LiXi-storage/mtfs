/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_TRACE_H__
#define __MTFS_TRACE_H__

#if defined(__linux__) && defined(__KERNEL__)
#include "mtfs_common.h"
#include "memory.h"
#include "debug.h"
#include <linux/file.h>
#include <linux/poll.h>

#ifdef HAVE_FILE_READV
extern ssize_t mtrace_file_readv(struct file *file, const struct iovec *iov,
                                 unsigned long nr_segs, loff_t *ppos);
#else /* ! HAVE_FILE_READV */
extern ssize_t mtrace_file_aio_read(struct kiocb *iocb, const struct iovec *iov,
                                    unsigned long nr_segs, loff_t pos);
#endif /* ! HAVE_FILE_WRITEV */
#ifdef HAVE_FILE_WRITEV
extern ssize_t mtrace_file_writev(struct file *file, const struct iovec *iov,
                                  unsigned long nr_segs, loff_t *ppos);
#else /* ! HAVE_FILE_WRITEV */
extern ssize_t mtrace_file_aio_write(struct kiocb *iocb, const struct iovec *iov,
                                     unsigned long nr_segs, loff_t pos);
#endif /* ! HAVE_FILE_WRITEV */

#else /* !defined (__linux__) && defined(__KERNEL__) */
#error This head is only for kernel space use
#endif /* !defined (__linux__) && defined(__KERNEL__) */

#endif /* __MTFS_TRACE_H__ */
