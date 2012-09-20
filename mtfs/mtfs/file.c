/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <linux/dcache.h>
#include <linux/fs_stack.h>
#include <linux/module.h>
#include <linux/uio.h>
#include <linux/mount.h>
#include <linux/sched.h>
#include <mtfs_oplist.h>
#include <mtfs_ioctl.h>
#include <mtfs_device.h>
#include <mtfs_lock.h>
#include "dentry_internal.h"
#include "file_internal.h"
#include "support_internal.h"
#include "mmap_internal.h"
#include "io_internal.h"

#define READ 0
#define WRITE 1

int mtfs_file_dump(struct file *file)
{
	int ret = 0;
	struct dentry *dentry = NULL;

	if (!file || IS_ERR(file)) {
		HDEBUG("file: error = %ld\n", PTR_ERR(file));
		ret = -1;
		goto out;
	}

	HDEBUG("file: f_mode = 0x%x, f_flags = 0%o, f_count = %ld, "
		"f_version = %llu, f_pos = %llu\n",
		file->f_mode, file->f_flags, (long)file_count(file),
		(u64)file->f_version, file->f_pos);
	
	dentry = file->f_dentry;
	if (dentry && !IS_ERR(dentry)) {
		ret = mtfs_dentry_dump(dentry);
	}
out:
	return ret;
}

static int mtfs_f_alloc(struct file *file, int bnum)
{
	struct mtfs_file_info *f_info = NULL;
	int ret = 0;
	
	HASSERT(file);
	HASSERT(bnum > 0 && bnum <= MTFS_BRANCH_MAX);

	MTFS_SLAB_ALLOC_PTR(f_info, mtfs_file_info_cache);
	if (unlikely(f_info == NULL)) {
		ret = -ENOMEM;
		goto out;
	}
	f_info->bnum = bnum;
	
	_mtfs_f2info(file) = f_info;
out:
	return ret;
}

static int mtfs_f_free(struct file *file)
{
	struct mtfs_file_info *f_info = NULL;
	int ret = 0;
	
	HASSERT(file);
	f_info = mtfs_f2info(file);
	HASSERT(f_info);

	MTFS_SLAB_FREE_PTR(f_info, mtfs_file_info_cache);
	_mtfs_f2info(file) = NULL;
	HASSERT(_mtfs_f2info(file) == NULL);
	return ret;
}

int mtfs_readdir_branch(struct file *file, void *dirent, filldir_t filldir, mtfs_bindex_t bindex)
{
	int ret = 0;
	struct file *hidden_file = mtfs_f2branch(file, bindex);
	HENTRY();

	if (hidden_file) {
		hidden_file->f_pos = file->f_pos;
		ret = vfs_readdir(hidden_file, filldir, dirent);
		file->f_pos = hidden_file->f_pos;
	} else {
		HDEBUG("branch[%d] of file [%*s] is %s\n",
		       bindex, file->f_dentry->d_name.len, file->f_dentry->d_name.name,
		       "NULL");
		ret = -ENOENT;
	}

	HRETURN(ret);
}

int mtfs_readdir(struct file *file, void *dirent, filldir_t filldir)
{
	int ret = 0;
	struct file *hidden_file = NULL;
	struct mtfs_operation_list *list = NULL;
	mtfs_bindex_t bindex = 0;
	mtfs_bindex_t i = 0;
	HENTRY();

	HDEBUG("readdir [%*s]\n", file->f_dentry->d_name.len, file->f_dentry->d_name.name);

	list = mtfs_oplist_build(file->f_dentry->d_inode);
	if (unlikely(list == NULL)) {
		HERROR("failed to build operation list\n");
		ret = -ENOMEM;
		goto out;
	}

	if (list->latest_bnum == 0) {
		HERROR("dir [%*s] has no valid branch, try to read broken branches\n",
		       file->f_dentry->d_name.len, file->f_dentry->d_name.name);
	}

	for (i = 0; i < mtfs_f2bnum(file); i++) {
		bindex = list->op_binfo[i].bindex;
		ret = mtfs_readdir_branch(file, dirent, filldir, bindex);
		if (ret == 0) {
			break;
		}

		if (list->latest_bnum > 0 && i == list->latest_bnum - 1) {
			HERROR("dir [%*s] has no readable valid branch, try to read broken branches\n",
			       file->f_dentry->d_name.len, file->f_dentry->d_name.name);
		}
	}

	if (ret == 0) {
		hidden_file = mtfs_f2branch(file, bindex);
		fsstack_copy_attr_atime(file->f_dentry->d_inode, hidden_file->f_dentry->d_inode);
	}
//out_free_oplist:
	mtfs_oplist_free(list);
out:
	HRETURN(ret);
}
EXPORT_SYMBOL(mtfs_readdir);

unsigned int mtfs_poll(struct file *file, poll_table *wait)
{
/*	unsigned int mask = DEFAULT_POLLMASK;
	struct file *hidden_file = NULL;

	if (mtfs_f2info(file) != NULL)
		hidden_file = mtfs_f2branch(file, 0);

	if (!hidden_file->f_op || !hidden_file->f_op->poll)
		goto out;

	mask = hidden_file->f_op->poll(hidden_file, wait);

out:
		return mask;*/
	return -ENODEV;
}
EXPORT_SYMBOL(mtfs_poll);

int mtfs_open_branch(struct inode *inode, struct file *file, mtfs_bindex_t bindex)
{
	struct dentry *dentry = file->f_dentry;
	struct vfsmount *hidden_mnt = mtfs_s2mntbranch(inode->i_sb, bindex);
	struct dentry *hidden_dentry = mtfs_d2branch(dentry, bindex);
	int hidden_flags = file->f_flags;
	struct file *hidden_file = NULL;
	int ret = 0;
	HENTRY();

	if (hidden_dentry && hidden_dentry->d_inode) {
		dget(hidden_dentry);
		mntget(hidden_mnt);
		/*
		 * dentry_open will dput and mntput if error.
		 * Otherwise fput() will do an dput and mntput for us upon file close.
		 */
#ifdef HAVE_DENTRY_OPEN_4ARGS
		hidden_file = dentry_open(hidden_dentry, hidden_mnt, hidden_flags, current_cred());
#else /* HAVE_DENTRY_OPEN_4ARGS */
		hidden_file = dentry_open(hidden_dentry, hidden_mnt, hidden_flags);
#endif /* HAVE_DENTRY_OPEN_4ARGS */
		if (IS_ERR(hidden_file)) {
			HDEBUG("open branch[%d] of file [%*s], flags = 0x%x, ret = %ld\n", 
			       bindex, hidden_dentry->d_name.len, hidden_dentry->d_name.name,
			       hidden_flags, PTR_ERR(hidden_file));
			ret = PTR_ERR(hidden_file);
		} else {
			mtfs_f2branch(file, bindex) = hidden_file;
		}
	} else {
		HDEBUG("branch[%d] of dentry [%*s] is %s\n",
		       bindex, dentry->d_name.len, dentry->d_name.name,
		       hidden_dentry == NULL ? "NULL" : "negative");
		ret = -ENOENT;
	}

	HRETURN(ret);
}

int mtfs_open(struct inode *inode, struct file *file)
{
	int ret = -ENOMEM;
	struct file *hidden_file = NULL;
	mtfs_bindex_t i = 0;
	mtfs_bindex_t bindex = 0;
	mtfs_bindex_t bnum = 0;
	struct mtfs_operation_list *list = NULL;
	mtfs_operation_result_t result = {0};
	HENTRY();

	HASSERT(inode);
	HASSERT(file->f_dentry);

	list = mtfs_oplist_build(inode);
	if (unlikely(list == NULL)) {
		HERROR("failed to build operation list\n");
		ret = -ENOMEM;
		goto out;
	}

	if (list->latest_bnum == 0) {
		HERROR("file [%*s] has no valid branch, please check it\n",
		       file->f_dentry->d_name.len, file->f_dentry->d_name.name);
		if (!(mtfs_i2dev(inode)->no_abort)) {
			ret = -EIO;
			goto out_free_oplist;
		}
	}

	bnum = mtfs_i2bnum(inode);
	ret = mtfs_f_alloc(file, bnum);
	if (unlikely(ret)) {
		goto out_free_oplist;
	}

	for (i = 0; i < bnum; i++) {
		bindex = list->op_binfo[i].bindex;
		ret = mtfs_open_branch(inode, file, bindex);
		result.ret = ret;
		mtfs_oplist_setbranch(list, i, (ret == 0 ? 1 : 0), result);
		if (i == list->latest_bnum - 1) {
			mtfs_oplist_check(list);
			if (list->success_latest_bnum <= 0) {
				HDEBUG("operation failed for all latest branches\n");
				if (!(mtfs_i2dev(inode)->no_abort)) {
					result = mtfs_oplist_result(list);
					ret = result.ret;
					goto out_free_file;
				}
			}
		}
	}

	mtfs_oplist_check(list);
	if (list->success_bnum <= 0) {
		result = mtfs_oplist_result(list);
		ret = result.ret;
		goto out_free_file;
	}
	ret = 0;

	goto out_free_oplist;
out_free_file:
	for (bindex = 0; bindex < bnum; bindex++) {
		hidden_file = mtfs_f2branch(file, bindex);
		if (hidden_file) {
			/* fput will calls mntput and dput for lowerfs */
			fput(hidden_file);  
		}
	}
	mtfs_f_free(file);
out_free_oplist:
	mtfs_oplist_free(list);
out:
	HRETURN(ret);
}
EXPORT_SYMBOL(mtfs_open);

int mtfs_flush_branch(struct file *file, fl_owner_t id, mtfs_bindex_t bindex)
{
	struct file *hidden_file = mtfs_f2branch(file, bindex);
	int ret = 0;
	HENTRY();

	if (hidden_file) {
		HASSERT(hidden_file->f_op);
		HASSERT(hidden_file->f_op->flush);
		ret = hidden_file->f_op->flush(hidden_file, id);
		if (ret) {
			HDEBUG("failed to open branch[%d] of file [%*s], ret = %d\n", 
			       bindex, file->f_dentry->d_name.len, file->f_dentry->d_name.name,
			       ret);
		}
	} else {
		HDEBUG("branch[%d] of file [%*s] is NULL\n",
		       bindex, file->f_dentry->d_name.len, file->f_dentry->d_name.name);
		ret = -ENOENT;
	}
	HRETURN(ret);
}

int mtfs_flush(struct file *file, fl_owner_t id)
{
	int ret = 0;
	mtfs_bindex_t bindex;
	HENTRY();

	HASSERT(mtfs_f2info(file));
	for (bindex = 0; bindex < mtfs_f2bnum(file); bindex++) {
		ret = mtfs_flush_branch(file, id, bindex);
	}

	HRETURN(ret);
}
EXPORT_SYMBOL(mtfs_flush);

int mtfs_release_branch(struct inode *inode, struct file *file, mtfs_bindex_t bindex)
{
	struct file *hidden_file = mtfs_f2branch(file, bindex);
	int ret = 0;
	HENTRY();

	if (hidden_file) {
		/*
		 * Will decrement file reference,
		 * and if it count down to zero, destroy the file,
		 * which will call the lower file system's file release function.
		 */
		fput(hidden_file);
	} else {
		HDEBUG("branch[%d] of file [%*s] is NULL\n",
		       bindex, file->f_dentry->d_name.len, file->f_dentry->d_name.name);
		ret = -ENOENT;
	}

	HRETURN(ret);
}

int mtfs_release(struct inode *inode, struct file *file)
{
	int ret = 0;
	mtfs_bindex_t bindex = 0;
	mtfs_bindex_t bnum = 0;
	HENTRY();

	HDEBUG("release file [%*s]\n",
	       file->f_dentry->d_name.len, file->f_dentry->d_name.name);

	/* #BUG(posix:80): empty symlink */
	if (mtfs_f2info(file) == NULL) {
		HERROR("file [%*s] has no private data\n",
		       file->f_dentry->d_name.len, file->f_dentry->d_name.name);
		goto out;
	}

	/* fput all the hidden files */
	bnum = mtfs_f2bnum(file);
	for (bindex = 0; bindex < bnum; bindex++) {
		ret = mtfs_release_branch(inode, file, bindex);
	}
	mtfs_f_free(file);

out:
	HRETURN(ret);
}
EXPORT_SYMBOL(mtfs_release);

int mtfs_fsync_branch(struct file *file, struct dentry *dentry, int datasync, mtfs_bindex_t bindex)
{
	int ret = 0;
	struct file *hidden_file = mtfs_f2branch(file, bindex);
	struct dentry *hidden_dentry = mtfs_d2branch(dentry, bindex);
	HENTRY();

	if (hidden_file && hidden_dentry) {
		HASSERT(hidden_file->f_op);
		HASSERT(hidden_file->f_op->fsync);
		mutex_lock(&hidden_dentry->d_inode->i_mutex);
		ret = hidden_file->f_op->fsync(hidden_file, hidden_dentry, datasync);
		mutex_unlock(&hidden_dentry->d_inode->i_mutex);
		if (unlikely(ret)) {
			HDEBUG("failed to fsync branch[%d] of file [%*s], ret = %d\n", 
			       bindex, file->f_dentry->d_name.len, file->f_dentry->d_name.name,
			       ret);
		}
	} else {
		HASSERT(hidden_file == NULL);
		HASSERT(hidden_dentry == NULL);
		HDEBUG("branch[%d] of file [%*s] is NULL\n",
		       bindex, dentry->d_name.len, dentry->d_name.name);
	}

	HRETURN(ret);
}

int mtfs_fsync(struct file *file, struct dentry *dentry, int datasync)
{
	int ret = -EINVAL;
	mtfs_bindex_t bindex = 0;
	HENTRY();

	HASSERT(inode_is_locked(file->f_mapping->host));
	HASSERT(inode_is_locked(dentry->d_inode));
	HASSERT(mtfs_f2info(file));

	for (bindex = 0; bindex < mtfs_f2bnum(file); bindex++) {
		ret = mtfs_fsync_branch(file, dentry, datasync, bindex);
	}

	HRETURN(ret);
}
EXPORT_SYMBOL(mtfs_fsync);

int mtfs_fasync_branch(int fd, struct file *file, int flag, mtfs_bindex_t bindex)
{
	int ret = 0;
	struct file *hidden_file = mtfs_f2branch(file, bindex);
	HENTRY();

	if (hidden_file) {
		HASSERT(hidden_file->f_op);
		HASSERT(hidden_file->f_op->fasync);
		ret = hidden_file->f_op->fasync(fd, hidden_file, flag);
	} else {
		HDEBUG("branch[%d] of file [%*s] is NULL\n",
		       bindex, file->f_dentry->d_name.len, file->f_dentry->d_name.name);
	}
	HRETURN(ret);
}

int mtfs_fasync(int fd, struct file *file, int flag)
{
	int ret = 0;
	mtfs_bindex_t bindex = 0;
	HENTRY();

	HASSERT(mtfs_f2info(file));
	for (bindex = 0; bindex < mtfs_f2bnum(file); bindex++) {
		ret = mtfs_fasync_branch(fd, file, flag, bindex);
	}

	HRETURN(ret);
}
EXPORT_SYMBOL(mtfs_fasync);

int mtfs_lock(struct file *file, int cmd, struct file_lock *fl)
{
	int ret = 0;
	struct file *hidden_file = NULL;
	mtfs_bindex_t bindex = 0;
	HENTRY();

	HASSERT(mtfs_f2info(file));
	for (bindex = 0; bindex < mtfs_f2bnum(file); bindex++) {
		hidden_file = mtfs_f2branch(file, bindex);
		if (hidden_file) {
			HASSERT(hidden_file->f_op);
			HASSERT(hidden_file->f_op->lock);
			if (hidden_file->f_op->lock) {
				ret = hidden_file->f_op->lock(hidden_file, F_GETLK, fl);
			}
		} else {
			HDEBUG("branch[%d] of file [%*s] is NULL\n",
			       bindex, file->f_dentry->d_name.len, file->f_dentry->d_name.name);
		}
	}

	HRETURN(ret);
}
EXPORT_SYMBOL(mtfs_lock);

int mtfs_flock(struct file *file, int cmd, struct file_lock *fl)
{
	int ret = 0;
	struct file *hidden_file = NULL;
	mtfs_bindex_t bindex = 0;
	HENTRY();

	HASSERT(mtfs_f2info(file));
	for (bindex = 0; bindex < mtfs_f2bnum(file); bindex++) {
		hidden_file = mtfs_f2branch(file, bindex);
		if (hidden_file) {
			HASSERT(hidden_file->f_op);
			HASSERT(hidden_file->f_op->flock);
			ret = hidden_file->f_op->flock(hidden_file, cmd, fl);
			if (ret) {
				/* What to do? */
			}
		} else {
			HDEBUG("branch[%d] of file [%*s] is NULL\n",
			       bindex, file->f_dentry->d_name.len, file->f_dentry->d_name.name);
		}
	}

	HRETURN(ret);
}
EXPORT_SYMBOL(mtfs_flock);

static size_t get_iov_count(const struct iovec *iov,
                            unsigned long *nr_segs)
{
	size_t count = 0;
	unsigned long seg = 0;

	for (seg = 0; seg < *nr_segs; seg++) {
		const struct iovec *iv = &iov[seg];

		/*
		 * If any segment has a negative length, or the cumulative
		 * length ever wraps negative then return -EINVAL.
		*/
		count += iv->iov_len;
		if (unlikely((ssize_t)(count|iv->iov_len) < 0))
			return -EINVAL;
		if (access_ok(VERIFY_WRITE, iv->iov_base, iv->iov_len))
			continue;
		if (seg == 0)
			return -EFAULT;
		*nr_segs = seg;
		count -= iv->iov_len;   /* This segment is no good */
		break;
	}
	return count;
}

#ifdef HAVE_FILE_READV
static ssize_t _do_readv_writev(int is_write, struct file *file, const struct iovec *iov,
                                unsigned long nr_segs, loff_t *ppos)
{
	ssize_t ret = 0;
	HENTRY();

	if (is_write) {
		HASSERT(file->f_op->writev);
		ret = file->f_op->writev(file, iov, nr_segs, ppos);
	} else {
		HASSERT(file->f_op->readv);
		ret = file->f_op->readv(file, iov, nr_segs, ppos);
	}

	HRETURN(ret);
}
#else  /* !HAVE_FILE_READV */
static ssize_t _do_readv_writev(int is_write, struct file *file, const struct iovec *iov,
                                unsigned long nr_segs, loff_t *ppos)
{
	ssize_t ret = 0;
	mm_segment_t old_fs;
	HENTRY();

	old_fs = get_fs();
	set_fs(get_ds());
	if (is_write) {
		/* Not possible to call ->aio_write or ->aio_read directly */
		HASSERT(file->f_op->aio_write);
		ret = vfs_writev(file, iov, nr_segs, ppos);
	} else {
		HASSERT(file->f_op->aio_read);
		ret = vfs_readv(file, iov, nr_segs, ppos);
	}
	set_fs(old_fs);

	HRETURN(ret);
}
#endif /* !HAVE_FILE_READV */

ssize_t mtfs_file_rw_branch(int is_write, struct file *file, const struct iovec *iov,
                            unsigned long nr_segs, loff_t *ppos, mtfs_bindex_t bindex)
{
	ssize_t ret = 0;
	struct file *hidden_file = mtfs_f2branch(file, bindex);
	struct inode *inode = NULL;
	struct inode *hidden_inode = NULL;
	HENTRY();

	if (is_write) {
		ret = mtfs_device_branch_errno(mtfs_f2dev(file), bindex, BOPS_MASK_WRITE);
	} else {
		ret = mtfs_device_branch_errno(mtfs_f2dev(file), bindex, BOPS_MASK_READ);
	}

	if (ret) {
		HDEBUG("branch[%d] is abandoned\n", bindex);
		goto out; 
	}

	if (hidden_file == NULL) {
		HERROR("branch[%d] of file [%*s] is NULL\n",
		       bindex, file->f_dentry->d_name.len,
		       file->f_dentry->d_name.name);
		ret = -ENOENT;
		goto out;
	}

	HASSERT(hidden_file->f_op);
	ret = _do_readv_writev(is_write, hidden_file, iov, nr_segs, ppos);
	if (ret > 0) {
		/*
		 * Lustre update inode size whenever read/write.
		 * TODO: Do not update unless file growes bigger.
		 */
		inode = file->f_dentry->d_inode;
		HASSERT(inode);
		hidden_inode = mtfs_i2branch(inode, bindex);
		HASSERT(hidden_inode);
		fsstack_copy_inode_size(inode, hidden_inode);
	}

out:
	HRETURN(ret);
}

static int mtfs_io_init_rw(struct mtfs_io *io, int is_write,
                           struct file *file, const struct iovec *iov,
                           unsigned long nr_segs, loff_t *ppos, size_t rw_size)
{
	int ret = 0;
	mtfs_io_type_t type;
	struct mtfs_io_rw *io_rw = &io->u.mi_rw;
	HENTRY();

	type = is_write ? MIT_WRITEV : MIT_READV;
	io->mi_ops = &mtfs_io_ops[type];
	io->mi_type = type;
	io->mi_oplist_dentry = file->f_dentry;
	io->mi_bindex = 0;
	io->mi_bnum = mtfs_f2bnum(file);
	io->mi_break = 0;
	io->mi_oplist_dentry = file->f_dentry;
	io->mi_resource = mtfs_i2resource(file->f_dentry->d_inode);
	io->mi_einfo.mode = is_write ? MLOCK_MODE_WRITE : MLOCK_MODE_READ;
	io->mi_einfo.data.mlp_extent.start = *ppos;
	io->mi_einfo.data.mlp_extent.end = *ppos + rw_size;

	io_rw->file = file;
	io_rw->iov = iov;
	io_rw->nr_segs = nr_segs;
	io_rw->ppos = ppos;

	io_rw->iov_length = sizeof(*iov) * nr_segs;
	MTFS_ALLOC(io_rw->iov_tmp, io_rw->iov_length);
	if (io_rw->iov_tmp == NULL) {
		HERROR("not enough memory\n");
		ret = -ENOMEM;
		goto out;
	}
	memcpy((char *)io_rw->iov_tmp, (char *)iov, io_rw->iov_length); 

out:
	HRETURN(ret);
}

static void mtfs_io_fini_rw(struct mtfs_io *io)
{
	struct mtfs_io_rw *io_rw = &io->u.mi_rw;
	HENTRY();

	MTFS_FREE(io_rw->iov_tmp, io_rw->iov_length);

	_HRETURN();
}

static ssize_t mtfs_file_rw(int is_write, struct file *file, const struct iovec *iov,
                            unsigned long nr_segs, loff_t *ppos)
{
	ssize_t size = 0;
	int ret = 0;
	struct mtfs_io *io = NULL;
	size_t rw_size = 0;
	HENTRY();

	rw_size = get_iov_count(iov, &nr_segs);
	if (rw_size <= 0) {
		ret = rw_size;
		goto out;
	}

	MTFS_SLAB_ALLOC_PTR(io, mtfs_io_cache);
	if (io == NULL) {
		HERROR("not enough memory\n");
		size = -ENOMEM;
		goto out;
	}

	ret = mtfs_io_init_rw(io, is_write, file, iov, nr_segs, ppos, rw_size);
	if (ret) {
		size = ret;
		goto out_free_io;
	}

	ret = mtfs_io_loop(io);
	if (ret) {
		HERROR("failed to loop on io\n");
		size = ret;
	} else {
		size = io->mi_result.size;
	}

	mtfs_io_fini_rw(io);
	*ppos = *ppos + size;
out_free_io:
	MTFS_SLAB_FREE_PTR(io, mtfs_io_cache);
out:
	HRETURN(size);
}

#ifdef HAVE_FILE_READV
ssize_t mtfs_file_readv(struct file *file, const struct iovec *iov,
                        unsigned long nr_segs, loff_t *ppos)
{
	ssize_t size = 0;
	HENTRY();

	size = mtfs_file_rw(READ, file, iov, nr_segs, ppos);

	HRETURN(size);
}
EXPORT_SYMBOL(mtfs_file_readv);

ssize_t mtfs_file_read(struct file *file, char __user *buf, size_t len,
                       loff_t *ppos)
{
	struct iovec local_iov = { .iov_base = (void __user *)buf,
	                           .iov_len = len };
	return mtfs_file_readv(file, &local_iov, 1, ppos);
}
EXPORT_SYMBOL(mtfs_file_read);

ssize_t mtfs_file_writev(struct file *file, const struct iovec *iov,
                        unsigned long nr_segs, loff_t *ppos)
{
	ssize_t size = 0;
	HENTRY();

	size = mtfs_file_rw(WRITE, file, iov, nr_segs, ppos);

	HRETURN(size);
}
EXPORT_SYMBOL(mtfs_file_writev);

ssize_t mtfs_file_write(struct file *file, const char __user *buf, size_t len, loff_t *ppos)
{
	struct iovec local_iov = { .iov_base = (void __user *)buf,
	                           .iov_len = len };
	HASSERT(file->f_op);
	HASSERT(file->f_op->writev);
	return file->f_op->writev(file, &local_iov, 1, ppos);	
}
EXPORT_SYMBOL(mtfs_file_write);

#else /* !HAVE_FILE_READV */

ssize_t mtfs_file_aio_read(struct kiocb *iocb, const struct iovec *iov,
                           unsigned long nr_segs, loff_t pos)
{
	ssize_t size = 0;
	struct file *file = iocb->ki_filp;
	HENTRY();

	size = mtfs_file_rw(READ, file, iov, nr_segs, &pos);
	if (size > 0) {
		iocb->ki_pos = pos + size;
	}

	HRETURN(size);
}
EXPORT_SYMBOL(mtfs_file_aio_read);

ssize_t mtfs_file_read(struct file *file, char __user *buf, size_t len, loff_t *ppos)
{
	struct iovec local_iov = { .iov_base = (void __user *)buf,
	                           .iov_len = len };
	struct kiocb kiocb;
	ssize_t ret = 0;
	HENTRY();

	init_sync_kiocb(&kiocb, file);
	kiocb.ki_pos = *ppos;
	kiocb.ki_left = len;
	HASSERT(file->f_op);
	HASSERT(file->f_op->aio_read);
	ret = file->f_op->aio_read(&kiocb, &local_iov, 1, *ppos);
	if (ret > 0) {
		*ppos = *ppos + ret;
	}

	HRETURN(ret);
}
EXPORT_SYMBOL(mtfs_file_read);

ssize_t mtfs_file_aio_write(struct kiocb *iocb, const struct iovec *iov,
                            unsigned long nr_segs, loff_t pos)
{
	ssize_t size = 0;
	struct file *file = iocb->ki_filp;
	HENTRY();

	size = mtfs_file_rw(WRITE, file, iov, nr_segs, &pos);
	if (size > 0) {
		iocb->ki_pos = pos + size;
	}

	HRETURN(size);
}
EXPORT_SYMBOL(mtfs_file_aio_write);

ssize_t mtfs_file_write(struct file *file, const char __user *buf, size_t len, loff_t *ppos)
{
	struct iovec local_iov = { .iov_base = (void __user *)buf,
	                           .iov_len = len };
	struct kiocb kiocb;
	ssize_t ret = 0;
	HENTRY();

	init_sync_kiocb(&kiocb, file);
	kiocb.ki_pos = *ppos;
	kiocb.ki_left = len;
	HASSERT(file->f_op);
	HASSERT(file->f_op->aio_write);
	ret = file->f_op->aio_write(&kiocb, &local_iov, 1, *ppos);
	if (ret > 0) {
		*ppos = *ppos + ret;
	}

	HRETURN(ret);
}
EXPORT_SYMBOL(mtfs_file_write);

#endif /* !HAVE_FILE_READV */

static ssize_t mtfs_file_write_nonwritev_branch(struct file *file, const char __user *buf, size_t len,
                                                loff_t *ppos, mtfs_bindex_t bindex)
{
	ssize_t ret = 0;
	struct file *hidden_file = mtfs_f2branch(file, bindex);
	struct inode *inode = NULL;
	struct inode *hidden_inode = NULL;
	HENTRY();

	if (hidden_file) {
		HASSERT(hidden_file->f_op);
		HASSERT(hidden_file->f_op->write);
		ret = hidden_file->f_op->write(hidden_file, buf, len, ppos);
		if (ret > 0) {
			/*
			 * Lustre update inode size whenever read/write.
			 * TODO: Do not update unless file growes bigger.
			 */
			inode = file->f_dentry->d_inode;
			HASSERT(inode);
			hidden_inode = mtfs_i2branch(inode, bindex);
			HASSERT(hidden_inode);
			fsstack_copy_inode_size(inode, hidden_inode);
		}
	} else {
		HERROR("branch[%d] of file [%*s] is NULL\n",
		       bindex, file->f_dentry->d_name.len, file->f_dentry->d_name.name);
		ret = -ENOENT;
	}

	HRETURN(ret);
}

ssize_t mtfs_file_write_nonwritev(struct file *file, const char __user *buf, size_t len, loff_t *ppos)
{
	ssize_t ret = 0;
	ssize_t success_ret = 0;
	mtfs_bindex_t bindex = 0;
	loff_t tmp_pos = 0;
	loff_t success_pos = 0;
	HENTRY();
	
	
	HASSERT(mtfs_f2info(file));
	for (bindex = 0; bindex < mtfs_f2bnum(file); bindex++) {
		tmp_pos = *ppos;
		ret = mtfs_file_write_nonwritev_branch(file, buf, len, &tmp_pos, bindex);
		HDEBUG("writed branch[%d] of file [%*s] at pos = %llu, ret = %ld\n",
		       bindex, file->f_dentry->d_name.len, file->f_dentry->d_name.name,
		       *ppos, ret);
		/* TODO: set data flags */
		if (ret >= 0) {
			success_pos = tmp_pos;
			success_ret = ret;
		}
	}

	if (success_ret >= 0) {
		ret = success_ret;
		*ppos = success_pos;
	}

	HRETURN(ret);
}
EXPORT_SYMBOL(mtfs_file_write_nonwritev);

static ssize_t mtfs_file_read_nonreadv_branch(struct file *file, char __user *buf, size_t len,
                                              loff_t *ppos, mtfs_bindex_t bindex)
{
	ssize_t ret = 0;
	struct file *hidden_file = mtfs_f2branch(file, bindex);
	struct inode *inode = NULL;
	struct inode *hidden_inode = NULL;
	HENTRY();

	if (hidden_file) {
		HASSERT(hidden_file->f_op);
		HASSERT(hidden_file->f_op->read);
		ret = hidden_file->f_op->read(hidden_file, buf, len, ppos);
		if (ret > 0) {
			/*
			 * Lustree update inode size whenever read/write.
			 * TODO: Do not update unless file growes bigger.
			 */
			inode = file->f_dentry->d_inode;
			HASSERT(inode);
			hidden_inode = mtfs_i2branch(inode, bindex);
			HASSERT(hidden_inode);
			fsstack_copy_inode_size(inode, hidden_inode);
		}
	} else {
		HERROR("branch[%d] of file [%*s] is NULL\n",
		       bindex, file->f_dentry->d_name.len, file->f_dentry->d_name.name);
		ret = -ENOENT;
	}

	HRETURN(ret);
}

ssize_t mtfs_file_read_nonreadv(struct file *file, char __user *buf, size_t len,
                                loff_t *ppos)
{
	ssize_t ret = 0;
	mtfs_bindex_t bindex = 0;
	loff_t tmp_pos = 0;
	HENTRY();

	HASSERT(mtfs_f2info(file));
	for (bindex = 0; bindex < mtfs_f2bnum(file); bindex++) {
		tmp_pos = *ppos;
		ret = mtfs_file_read_nonreadv_branch(file, buf, len, &tmp_pos, bindex);
		HDEBUG("readed branch[%d] of file [%*s] at pos = %llu, ret = %ld\n",
		       bindex, file->f_dentry->d_name.len, file->f_dentry->d_name.name,
		       *ppos, ret);
		if (ret >= 0) { 
			*ppos = tmp_pos;
			break;
		}
	}

	HRETURN(ret);
}
EXPORT_SYMBOL(mtfs_file_read_nonreadv);

loff_t mtfs_file_llseek(struct file *file, loff_t offset, int origin)
{
	loff_t ret = 0;
	HENTRY();

	ret = generic_file_llseek(file, offset, origin);

	HRETURN(ret);
}
EXPORT_SYMBOL(mtfs_file_llseek);

int mtfs_file_mmap(struct file *file, struct vm_area_struct * vma)
{
	int ret = 0;
	struct mtfs_operations *operations = NULL;
	HENTRY();

	ret = generic_file_mmap(file, vma);
	if (ret) {
		goto out;
	}

	operations = mtfs_f2ops(file);
	if (operations->vm_ops) {
		vma->vm_ops = operations->vm_ops;
	} else {
		HDEBUG("vm operations not supplied, use default\n");
		vma->vm_ops = &mtfs_file_vm_ops;
	}

	/* Lusre will update the inode's size and mtime */
out:
	HRETURN(ret);
}
EXPORT_SYMBOL(mtfs_file_mmap);

int mtfs_file_mmap_nowrite(struct file *file, struct vm_area_struct * vma)
{
	int ret = 0;
	struct mtfs_operations *operations = NULL;
	HENTRY();

	if ((vma->vm_flags & VM_WRITE) != 0) {
		ret = -EOPNOTSUPP;
		goto out;
	}
		
	ret = generic_file_mmap(file, vma);
	if (ret) {
		goto out;
	}

	operations = mtfs_f2ops(file);
	if (operations->vm_ops) {
		vma->vm_ops = operations->vm_ops;
	} else {
		HDEBUG("vm operations not supplied, use default\n");
		vma->vm_ops = &mtfs_file_vm_ops;
	}

	/* Lustre will update the inode's size and mtime */
out:
	HRETURN(ret);
}
EXPORT_SYMBOL(mtfs_file_mmap_nowrite);

#ifdef HAVE_KERNEL_SENDFILE
ssize_t mtfs_file_sendfile(struct file *in_file, loff_t *ppos,
                           size_t len, read_actor_t actor, void *target)
{
	ssize_t ret = 0;
	loff_t pos_tmp = 0;
	struct file *hidden_file = NULL;
	mtfs_bindex_t bindex = 0;
	HENTRY();

	HASSERT(mtfs_f2info(in_file));

	for (bindex = 0; bindex < mtfs_f2bnum(in_file); bindex++) {
		hidden_file = mtfs_f2branch(in_file, bindex);

		if (hidden_file) {
			HASSERT(hidden_file->f_op);
			HASSERT(hidden_file->f_op->sendfile);
			pos_tmp = *ppos;
			ret = hidden_file->f_op->sendfile(hidden_file, &pos_tmp, len, actor, target);
			if (ret >= 0) { 
				*ppos = pos_tmp;
				break;
			}
		} else {
			HDEBUG("branch[%d] of file [%*s] is NULL\n",
			       bindex, in_file->f_dentry->d_name.len, in_file->f_dentry->d_name.name);
		}
	}
	// generic_file_sendfile?
	HRETURN(ret);
}
EXPORT_SYMBOL(mtfs_file_sendfile);
#endif /* HAVE_KERNEL_SENDFILE */

struct file_operations mtfs_dir_fops =
{
	llseek:   mtfs_file_llseek,
	read:     generic_read_dir, /* always returns error */
	readdir:  mtfs_readdir,
	ioctl:    mtfs_ioctl,
	open:     mtfs_open,
	release:  mtfs_release,
	flush:    mtfs_flush,
};

struct file_operations mtfs_main_fops =
{
	llseek:     mtfs_file_llseek,
	read:       mtfs_file_read,
	write:      mtfs_file_write,
#ifdef HAVE_KERNEL_SENDFILE
	sendfile:   mtfs_file_sendfile, 
#endif /* HAVE_KERNEL_SENDFILE */
#ifdef HAVE_FILE_READV
	readv:      mtfs_file_readv,
	writev:     mtfs_file_writev,
#endif /* HAVE_FILE_READV */
	readdir:    mtfs_readdir,
	poll:       mtfs_poll,
	ioctl:      mtfs_ioctl,
	mmap:       mtfs_file_mmap,
	open:       mtfs_open,
//	flush:      mtfs_flush,
	release:    mtfs_release,
	fsync:      mtfs_fsync,
	fasync:     mtfs_fasync,
	lock:       mtfs_lock,
	flock:      mtfs_flock,
	/* not implemented: sendpage */
	/* not implemented: get_unmapped_area */
};
