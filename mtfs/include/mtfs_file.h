/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __HRFS_FILE_H__
#define __HRFS_FILE_H__

#if defined (__linux__) && defined(__KERNEL__)
#include "mtfs_common.h"
#include "memory.h"
#include "debug.h"

extern int mtfs_readdir(struct file *file, void *dirent, filldir_t filldir);
extern unsigned int mtfs_poll(struct file *file, poll_table *wait);
extern int mtfs_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);
extern int mtfs_open(struct inode *inode, struct file *file);
extern int mtfs_flush(struct file *file, fl_owner_t id);
extern int mtfs_release(struct inode *inode, struct file *file);
extern int mtfs_fsync(struct file *file, struct dentry *dentry, int datasync);
extern int mtfs_fasync(int fd, struct file *file, int flag);
extern int mtfs_lock(struct file *file, int cmd, struct file_lock *fl);
extern int mtfs_flock(struct file *file, int cmd, struct file_lock *fl);
extern ssize_t mtfs_file_readv(struct file *file, const struct iovec *iov,
                               unsigned long nr_segs, loff_t *ppos);
extern ssize_t mtfs_file_writev(struct file *file, const struct iovec *iov,
                                unsigned long nr_segs, loff_t *ppos);
extern ssize_t mtfs_file_write_nonwritev(struct file *file, const char __user *buf,
                                         size_t len, loff_t *ppos);
extern ssize_t mtfs_file_read_nonreadv(struct file *file, char __user *buf, size_t len,
                                       loff_t *ppos);
extern ssize_t mtfs_file_read(struct file *file, char __user *buf, size_t len,
                              loff_t *ppos);
extern ssize_t mtfs_file_write(struct file *file, const char __user *buf, size_t len, loff_t *ppos);
extern ssize_t mtfs_file_aio_read(struct kiocb *iocb, char __user *buf, size_t count, loff_t pos);
extern ssize_t mtfs_file_aio_write(struct kiocb *iocb, const char __user *buf,
                                   size_t count, loff_t pos);
extern loff_t mtfs_file_llseek(struct file *file, loff_t offset, int origin);
extern int mtfs_file_mmap(struct file * file, struct vm_area_struct * vma);
extern int mtfs_file_mmap_nowrite(struct file *file, struct vm_area_struct * vma);
extern ssize_t mtfs_file_sendfile(struct file *in_file, loff_t *ppos,
                                  size_t len, read_actor_t actor, void *target);
extern int mtfs_ioctl_write(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);

extern struct file_operations mtfs_main_fops;
extern struct file_operations mtfs_dir_fops;

/*
 * Be careful!
 * Keep the same with struct mtfs_file_branch defined in llite/file.c
 * If change this, change there too.
 * Alright, it is urgly.
 */
struct mtfs_file_branch {
	int is_used;
	int is_valid;
	struct file *bfile;
};

/* file private data. */
struct mtfs_file_info {
	mtfs_bindex_t bnum;
	struct mtfs_file_branch barray[HRFS_BRANCH_MAX];
};

/* DO NOT access mtfs_*_info_t directly, use following macros */
#define _mtfs_f2info(file) ((file)->private_data)
#define mtfs_f2info(file) ((struct mtfs_file_info *)_mtfs_f2info(file))
#define mtfs_f2bnum(file) (mtfs_f2info(file)->bnum)
#define mtfs_f2barray(file) (mtfs_f2info(file)->barray)
#define mtfs_f2branch(file, bindex) (mtfs_f2barray(file)[bindex].bfile)
#define mtfs_f2bvalid(file, bindex) (mtfs_f2barray(file)[bindex].is_valid)
#endif /* defined (__linux__) && defined(__KERNEL__) */
#endif /* __HRFS_FILE_H__ */