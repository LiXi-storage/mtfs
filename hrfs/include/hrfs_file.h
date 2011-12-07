/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
 */

#ifndef __HRFS_FILE_H__
#define __HRFS_FILE_H__

#if defined (__linux__) && defined(__KERNEL__)
#include <hrfs_common.h>
#include <memory.h>
#include <debug.h>

extern int hrfs_readdir(struct file *file, void *dirent, filldir_t filldir);
extern unsigned int hrfs_poll(struct file *file, poll_table *wait);
extern int hrfs_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);
extern int hrfs_open(struct inode *inode, struct file *file);
extern int hrfs_flush(struct file *file, fl_owner_t id);
extern int hrfs_release(struct inode *inode, struct file *file);
extern int hrfs_fsync(struct file *file, struct dentry *dentry, int datasync);
extern int hrfs_fasync(int fd, struct file *file, int flag);
extern int hrfs_lock(struct file *file, int cmd, struct file_lock *fl);
extern int hrfs_flock(struct file *file, int cmd, struct file_lock *fl);
extern ssize_t hrfs_file_readv(struct file *file, const struct iovec *iov,
                               unsigned long nr_segs, loff_t *ppos);
extern ssize_t hrfs_file_writev(struct file *file, const struct iovec *iov,
                                unsigned long nr_segs, loff_t *ppos);
extern ssize_t hrfs_file_write_nonwritev(struct file *file, const char __user *buf,
                                         size_t len, loff_t *ppos);
extern ssize_t hrfs_file_read_nonreadv(struct file *file, char __user *buf, size_t len,
                                       loff_t *ppos);
extern ssize_t hrfs_file_read(struct file *file, char __user *buf, size_t len,
                              loff_t *ppos);
extern ssize_t hrfs_file_write(struct file *file, const char __user *buf, size_t len, loff_t *ppos);
extern ssize_t hrfs_file_aio_read(struct kiocb *iocb, char __user *buf, size_t count, loff_t pos);
extern ssize_t hrfs_file_aio_write(struct kiocb *iocb, const char __user *buf,
                                   size_t count, loff_t pos);
extern loff_t hrfs_file_llseek(struct file *file, loff_t offset, int origin);
extern int hrfs_file_mmap(struct file * file, struct vm_area_struct * vma);
extern ssize_t hrfs_file_sendfile(struct file *in_file, loff_t *ppos,
                                  size_t len, read_actor_t actor, void *target);

extern struct file_operations hrfs_main_fops;
extern struct file_operations hrfs_dir_fops;

/*
 * Be careful!
 * Keep the same with struct hrfs_file_branch defined in llite/file.c
 * If change this, change there too.
 * Alright, it is urgly.
 */
typedef struct hrfs_file_branch {
	int is_used;
	int is_valid;
	struct file *bfile;
} hrfs_f_branch_t;

/* file private data. */
typedef struct hrfs_file_info {
	hrfs_bindex_t bnum;
	hrfs_f_branch_t *barray; /*TODO: change to barray[] */
} hrfs_f_info_t;

/* DO NOT access hrfs_*_info_t directly, use following macros */
#define _hrfs_f2info(file) ((file)->private_data)
#define hrfs_f2info(file) ((hrfs_f_info_t *)_hrfs_f2info(file))
#define hrfs_f2bnum(file) (hrfs_f2info(file)->bnum)
#define hrfs_f2barray(file) (hrfs_f2info(file)->barray)
#define hrfs_f2branch(file, bindex) (hrfs_f2barray(file)[bindex].bfile)
#define hrfs_f2bvalid(file, bindex) (hrfs_f2barray(file)[bindex].is_valid)
#endif /* defined (__linux__) && defined(__KERNEL__) */
#endif /* __HRFS_FILE_H__ */
