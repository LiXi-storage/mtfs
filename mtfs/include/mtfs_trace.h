/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_TRACE_H__
#define __MTFS_TRACE_H__

#define MTFS_RESERVE_RECORD "RECORD"

#if defined(__linux__) && defined(__KERNEL__)
#include <linux/file.h>
#include <linux/poll.h>
#include "mtfs_common.h"
#include "mtfs_record.h"

struct msubject_trace_info {
	struct mrecord_handle msti_handle;
	struct ctl_table_header *msti_ctl_table;
};

extern int mtrace_subject_init(struct super_block *sb);
extern int mtrace_subject_fini(struct super_block *sb);
extern struct mtfs_subject_operations mtrace_subject_ops;

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
#endif /* defined (__linux__) && defined(__KERNEL__) */

#endif /* __MTFS_TRACE_H__ */
