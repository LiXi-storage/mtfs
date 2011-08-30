/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
 */

#include "hrfs_internal.h"
#include <hrfs_ioctl.h>
#include <linux/dcache.h>
#include <linux/uio.h>

int hrfs_file_dump(struct file *file)
{
	int ret = 0;
	struct dentry *dentry = NULL;

	if (!file || IS_ERR(file)) {
		HDEBUG("file: error = %ld\n", PTR_ERR(file));
		ret = -1;
		goto out;
	}

	HDEBUG("file: f_mode = 0x%x, f_flags = 0%o, f_count = %ld, "
		"f_version = %lu, f_pos = %llu\n",
		file->f_mode, file->f_flags, (long)file_count(file),
		file->f_version, file->f_pos);
	
	dentry = file->f_dentry;
	if (dentry && !IS_ERR(dentry)) {
		ret = hrfs_dentry_dump(dentry);
	}
out:
	return ret;
}

int hrfs_readdir(struct file *file, void *dirent, filldir_t filldir)
{
	int ret = -ENOTDIR;
	struct file *hidden_file = NULL;
	HENTRY();

	hidden_file = hrfs_f2branch(file, hrfs_get_primary_bindex());
	hidden_file->f_pos = file->f_pos;
	ret = vfs_readdir(hidden_file, filldir, dirent);
	file->f_pos = hidden_file->f_pos;

	if (ret < 0) {
		goto out;
	} else {
		fsstack_copy_attr_atime(file->f_dentry->d_inode, hidden_file->f_dentry->d_inode);
	}

out:
	HRETURN(ret);
}
EXPORT_SYMBOL(hrfs_readdir);

unsigned int hrfs_poll(struct file *file, poll_table *wait)
{
/*	unsigned int mask = DEFAULT_POLLMASK;
	struct file *hidden_file = NULL;

	if (hrfs_f2info(file) != NULL)
		hidden_file = hrfs_f2branch(file, 0);

	if (!hidden_file->f_op || !hidden_file->f_op->poll)
		goto out;

	mask = hidden_file->f_op->poll(hidden_file, wait);

out:
		return mask;*/
	return -ENODEV;
}
EXPORT_SYMBOL(hrfs_poll);

int hrfs_open(struct inode *inode, struct file *file)
{
	int ret = -ENOMEM;
	int hidden_flags = 0;
	struct file *hidden_file = NULL;
	struct dentry *hidden_dentry = NULL;
	struct dentry *dentry = NULL;
	struct super_block *sb = NULL;
	hrfs_bindex_t bindex = 0;
	hrfs_bindex_t bnum = 0;
	HENTRY();

	dentry = file->f_dentry;
	HASSERT(dentry);
	
	bnum = hrfs_d2bnum(dentry);
	ret = hrfs_f_alloc(file, bnum);
	if (unlikely(ret)) {
		goto out;
	}
	
	hidden_flags = file->f_flags;
	HASSERT(inode);
	sb = inode->i_sb;
	ret = 0;
	for (bindex = 0; bindex < bnum; bindex++) {
		hidden_dentry = hrfs_d2branch(dentry, bindex);
		if (hidden_dentry == NULL || hidden_dentry->d_inode == NULL) {
			HDEBUG("branch[%d] of dentry [%*s] is %s\n",
			       bindex, dentry->d_name.len, dentry->d_name.name,
			       hidden_dentry == NULL ? "NULL" : "negative");
			if (hrfs_is_primary_bindex(bindex)) {
				/* primary branch, report failed */
				ret = -ENOENT; /* which errno? */
				goto out;
			} else {
				continue;
			}
		}

		dget(hidden_dentry);
		/*
		 * dentry_open will decrement mnt refcnt if err.
		 * otherwise fput() will do an mntput() for us upon file close.
		 */
		mntget(hrfs_s2mntbranch(sb, bindex));
		hidden_file = dentry_open(hidden_dentry, hrfs_s2mntbranch(sb, bindex), hidden_flags);
		if (IS_ERR(hidden_file)) {
			HDEBUG("open branch[%d] of file [%*s], flags = 0x%x, ret = %ld\n", 
			       bindex, hidden_dentry->d_name.len, hidden_dentry->d_name.name,
			       hidden_flags, PTR_ERR(hidden_file));
			ret =  PTR_ERR(hidden_file);
			goto out;
		}
		hrfs_f2branch(file, bindex) = hidden_file;
	}

out:
	/* freeing the allocated resources, and fput the opened files */
	if (ret < 0 && hrfs_f2info(file)) {
		for (; bindex >= 0; bindex--) {
			hidden_file  = hrfs_f2branch(file, bindex);
			if (hidden_file) {
				/* fput calls dput for hidden_dentry */
				fput(hidden_file);  
			}
		}
		hrfs_f_free(file);
	}

	HRETURN(ret);
}
EXPORT_SYMBOL(hrfs_open);

int hrfs_flush(struct file *file, fl_owner_t id)
{
	int ret = 0;
	struct file *hidden_file = NULL;
	hrfs_bindex_t bindex;
	HENTRY();

	HASSERT(hrfs_f2info(file));
	for (bindex = 0; bindex < hrfs_f2bnum(file); bindex++) {
		hidden_file = hrfs_f2branch(file, bindex);
		if (hidden_file) {
			HASSERT(hidden_file->f_op);
			if (hidden_file->f_op->flush) {
				ret = hidden_file->f_op->flush(hidden_file, id);
			} else {
				HDEBUG("branch[%d] of file [%*s] does not support flush\n",
				       bindex, file->f_dentry->d_name.len, file->f_dentry->d_name.name);
				//ret = -ENOMEM; /* Which errno? */
			}
		} else {
			HDEBUG("branch[%d] of file [%*s] is NULL\n",
			       bindex, file->f_dentry->d_name.len, file->f_dentry->d_name.name);
		}
	}

	HRETURN(ret);
}
EXPORT_SYMBOL(hrfs_flush);

int hrfs_release(struct inode *inode, struct file *file)
{
	int err = 0;
	struct file *hidden_file = NULL;
	hrfs_bindex_t bindex = 0;
	hrfs_bindex_t bnum = 0;
	HENTRY();

	/*finish pending writes*/
	if (hrfs_s2info(file->f_dentry->d_sb)->is_lower_nfs) {
		if (file->f_mode & FMODE_WRITE) {
			filemap_fdatawrite(file->f_mapping);
		}
	}
	
	/* #BUG(posix:80): empty symlink */
	if (hrfs_f2info(file) == NULL) {
		HERROR("file [%*s] has no private data\n",
		       file->f_dentry->d_name.len, file->f_dentry->d_name.name);
		return 0;
	}

	/*fist_checkinode(inode, "hrfs_release");*/
	/* fput all the hidden files */
	bnum = hrfs_f2bnum(file);
	for (bindex = 0; bindex < bnum; bindex++) {
		hidden_file = hrfs_f2branch(file, bindex);
		if (hidden_file) {
			/* will decrement file refcount, and if 0, destroy the file,
			 * which will call the lower file system's file release function. */
			fput(hidden_file);
		} else {
			HDEBUG("branch[%d] of file [%*s] is NULL\n",
			       bindex, file->f_dentry->d_name.len, file->f_dentry->d_name.name);
		}
	}
	hrfs_f_free(file);

	HRETURN(err);
}
EXPORT_SYMBOL(hrfs_release);

int hrfs_fsync(struct file *file, struct dentry *dentry, int datasync)
{
	int ret = -EINVAL;
	struct file *hidden_file = NULL;
	struct dentry *hidden_dentry = NULL;
	hrfs_bindex_t bindex = 0;
	HENTRY();

	HASSERT(inode_is_locked(file->f_mapping->host));
	HASSERT(inode_is_locked(dentry->d_inode));

	HASSERT(hrfs_f2info(file));
	for (bindex = 0; bindex < hrfs_f2bnum(file); bindex++) {
		hidden_file = hrfs_f2branch(file, bindex);
		hidden_dentry = hrfs_d2branch(dentry, bindex);
		if (hidden_file && hidden_dentry) {
			HASSERT(hidden_file->f_op);
			HASSERT(hidden_file->f_op->fsync);
			mutex_lock(&hidden_dentry->d_inode->i_mutex);
			ret = hidden_file->f_op->fsync(hidden_file, hidden_dentry, datasync);
			mutex_unlock(&hidden_dentry->d_inode->i_mutex);
		} else {
			HASSERT(hidden_file == NULL);
			HASSERT(hidden_dentry == NULL);
			HDEBUG("branch[%d] of file [%*s] is NULL\n",
			       bindex, file->f_dentry->d_name.len, file->f_dentry->d_name.name);
		}
	}

	HRETURN(ret);
}
EXPORT_SYMBOL(hrfs_fsync);

int hrfs_fasync(int fd, struct file *file, int flag)
{
	int ret = 0;
	struct file *hidden_file = NULL;
	hrfs_bindex_t bindex = 0;
	HENTRY();

	HASSERT(hrfs_f2info(file));
	for (bindex = 0; bindex < hrfs_f2bnum(file); bindex++) {
		hidden_file = hrfs_f2branch(file, bindex);
		if (hidden_file) {
			HASSERT(hidden_file->f_op);
			if (hidden_file->f_op->fasync) {
				ret = hidden_file->f_op->fasync(fd, hidden_file, flag);
			} else {
				HDEBUG("branch[%d] of file [%*s] does not support fasync\n",
				       bindex, file->f_dentry->d_name.len, file->f_dentry->d_name.name);
				ret = -ENOMEM; /* Which errno? */
			}
		} else {
			HDEBUG("branch[%d] of file [%*s] is NULL\n",
			       bindex, file->f_dentry->d_name.len, file->f_dentry->d_name.name);
		}
	}
	
	HRETURN(ret);
}
EXPORT_SYMBOL(hrfs_fasync);

int hrfs_lock(struct file *file, int cmd, struct file_lock *fl)
{
	struct file_lock conflock;
	int ret = 0;
	struct file *hidden_file = NULL;
	hrfs_bindex_t bindex = 0;
	HENTRY();

	HASSERT(hrfs_f2info(file));
	for (bindex = 0; bindex < hrfs_f2bnum(file); bindex++) {
		hidden_file = hrfs_f2branch(file, bindex);
		if (hidden_file) {
			HASSERT(hidden_file->f_op);
			if (hidden_file->f_op->lock) {
				ret = hidden_file->f_op->lock(hidden_file, F_GETLK, fl);
			} else {
				posix_test_lock(hidden_file, fl, &conflock);
			}
		} else {
			HDEBUG("branch[%d] of file [%*s] is NULL\n",
			       bindex, file->f_dentry->d_name.len, file->f_dentry->d_name.name);
		}
	}

	HRETURN(ret);
}
EXPORT_SYMBOL(hrfs_lock);

int hrfs_flock(struct file *file, int cmd, struct file_lock *fl)
{
	int ret = 0;
	struct file *hidden_file = NULL;
	hrfs_bindex_t bindex = 0;
	HENTRY();

	HASSERT(hrfs_f2info(file));
	for (bindex = 0; bindex < hrfs_f2bnum(file); bindex++) {
		hidden_file = hrfs_f2branch(file, bindex);
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
EXPORT_SYMBOL(hrfs_flock);

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

size_t hrfs_file_readv_branch(struct file *file, const struct iovec *iov,
                                     unsigned long nr_segs, loff_t *ppos, hrfs_bindex_t bindex)
{
	ssize_t ret = 0;
	struct file *hidden_file = hrfs_f2branch(file, bindex);
	struct inode *inode = NULL;
	struct inode *hidden_inode = NULL;
	HENTRY();

	if (hidden_file) {
		HASSERT(hidden_file->f_op);
		HASSERT(hidden_file->f_op->readv);
		ret = hidden_file->f_op->readv(hidden_file, iov, nr_segs, ppos);
		if (ret > 0) {
			/*
			 * Swgfs update inode size whenever read/write.
			 * TODO: Do not update unless file growes bigger.
			 */
			inode = file->f_dentry->d_inode;
			HASSERT(inode);
			hidden_inode = hrfs_i2branch(inode, bindex);
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

ssize_t hrfs_file_readv(struct file *file, const struct iovec *iov,
                        unsigned long nr_segs, loff_t *ppos)
{ 
	ssize_t ret = 0;
	struct iovec *iov_new = NULL;
	struct iovec *iov_tmp = NULL;
	size_t length = sizeof(*iov) * nr_segs;
	size_t count = 0;
	hrfs_bindex_t bindex = 0;
	loff_t tmp_pos = 0;
	HENTRY();

	count = get_iov_count(iov, &nr_segs);
	/*
	 * "If nbyte is 0, read() will return 0 and have no other results."
	 *						-- Single Unix Spec
	 */
	if (count <= 0) {
		ret = count;
		goto out;
	}
	
	HRFS_ALLOC(iov_new, length);
	if (!iov_new) {
		ret = -ENOMEM;
		goto out;
	}

	HRFS_ALLOC(iov_tmp, length);
	if (!iov_tmp) {
		ret = -ENOMEM;
		goto new_alloced_err;
	}	
	memcpy((char *)iov_new, (char *)iov, length); 

	HASSERT(hrfs_f2info(file));
	for (bindex = 0; bindex < hrfs_f2bnum(file); bindex++) {
		memcpy((char *)iov_tmp, (char *)iov_new, length);
		tmp_pos = *ppos;
		ret = hrfs_file_readv_branch(file, iov_tmp, nr_segs, &tmp_pos, bindex);
		HDEBUG("readed branch[%d] of file [%*s] at pos = %llu, ret = %ld\n",
		       bindex, file->f_dentry->d_name.len, file->f_dentry->d_name.name,
		       *ppos, ret);
		if (ret >= 0) { 
			*ppos = tmp_pos;
			break;
		}
	}

//tmp_alloced_err:
	HRFS_FREE(iov_tmp, length);
new_alloced_err:
	HRFS_FREE(iov_new, length);
out:
	HRETURN(ret);
}
EXPORT_SYMBOL(hrfs_file_readv);

static ssize_t hrfs_file_writev_branch(struct file *file, const struct iovec *iov,
                                       unsigned long nr_segs, loff_t *ppos, hrfs_bindex_t bindex)
{
	ssize_t ret = 0;
	struct file *hidden_file = hrfs_f2branch(file, bindex);
	struct inode *inode = NULL;
	struct inode *hidden_inode = NULL;
	HENTRY();

	if (hidden_file) {
		HASSERT(hidden_file->f_op);
		HASSERT(hidden_file->f_op->writev);
		ret = hidden_file->f_op->writev(hidden_file, iov, nr_segs, ppos);
		if (ret > 0) {
			/*
			 * Swgfs update inode size whenever read/write.
			 * TODO: Do not update unless file growes bigger.
			 */
			inode = file->f_dentry->d_inode;
			HASSERT(inode);
			hidden_inode = hrfs_i2branch(inode, bindex);
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

ssize_t hrfs_file_writev(struct file *file, const struct iovec *iov,
                         unsigned long nr_segs, loff_t *ppos)
{
	ssize_t ret = 0;
	ssize_t success_ret = 0;
	struct iovec *iov_new = NULL;
	struct iovec *iov_tmp = NULL;
	size_t length = sizeof(*iov) * nr_segs;
	hrfs_bindex_t bindex = 0;
	loff_t tmp_pos = 0;
	loff_t success_pos = 0;
	HENTRY();
	
	HRFS_ALLOC(iov_new, length);
	if (!iov_new) {
		ret = -ENOMEM;
		goto out;
	}
	
	HRFS_ALLOC(iov_tmp, length);
	if (!iov_tmp) {
		ret = -ENOMEM;
		goto new_alloced_err;
	}	
	memcpy((char *)iov_new, (char *)iov, length); 
	
	HASSERT(hrfs_f2info(file));
	for (bindex = 0; bindex < hrfs_f2bnum(file); bindex++) {
		memcpy((char *)iov_tmp, (char *)iov_new, length);
		tmp_pos = *ppos;
		ret = hrfs_file_writev_branch(file, iov_tmp, nr_segs, &tmp_pos, bindex);
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
//tmp_alloced_err:
	HRFS_FREE(iov_tmp, length);
new_alloced_err:
	HRFS_FREE(iov_new, length);
out:
	HRETURN(ret);
}
EXPORT_SYMBOL(hrfs_file_writev);

ssize_t hrfs_file_read(struct file *file, char __user *buf, size_t len,
                       loff_t *ppos)
{
	struct iovec local_iov = { .iov_base = (void __user *)buf,
								.iov_len = len };
	return hrfs_file_readv(file, &local_iov, 1, ppos);
}
EXPORT_SYMBOL(hrfs_file_read);

ssize_t hrfs_file_write(struct file *file, const char __user *buf, size_t len, loff_t *ppos)
{
	struct iovec local_iov = { .iov_base = (void __user *)buf,
								.iov_len = len };
	return hrfs_file_writev(file, &local_iov, 1, ppos);	
}
EXPORT_SYMBOL(hrfs_file_write);

ssize_t
hrfs_file_aio_read(struct kiocb *iocb, char __user *buf, size_t count, loff_t pos)
{
	ssize_t err = 0;
	HENTRY();

	err = generic_file_aio_read(iocb, buf, count, pos);

	HRETURN(err);
}
EXPORT_SYMBOL(hrfs_file_aio_read);

ssize_t hrfs_file_aio_write(struct kiocb *iocb, const char __user *buf,
                            size_t count, loff_t pos)
{
	ssize_t ret = 0;
	HENTRY();

	ret = generic_file_aio_write(iocb, buf, count, pos);

	HRETURN(ret);
}
EXPORT_SYMBOL(hrfs_file_aio_write);

loff_t hrfs_file_llseek(struct file *file, loff_t offset, int origin)
{
	loff_t ret = 0;
	HENTRY();

	ret = generic_file_llseek(file, offset, origin);

	HRETURN(ret);
}
EXPORT_SYMBOL(hrfs_file_llseek);

int hrfs_file_mmap(struct file * file, struct vm_area_struct * vma)
{
	int ret = 0;
	HENTRY();

	ret = generic_file_mmap(file, vma);
	if (ret == 0) {
		vma->vm_ops = &hrfs_file_vm_ops;
		/* Swgfs will update the inode's size and mtime */
	}

	HRETURN(ret);
}
EXPORT_SYMBOL(hrfs_file_mmap);

#ifndef LIXI_20110928
ssize_t hrfs_file_sendfile(struct file *in_file, loff_t *ppos,
                           size_t len, read_actor_t actor, void *target)
{
	ssize_t ret = 0;
	loff_t pos_tmp = 0;
	struct file *hidden_file = NULL;
	hrfs_bindex_t bindex = 0;
	HENTRY();

	HASSERT(hrfs_f2info(in_file));

	for (bindex = 0; bindex < hrfs_f2bnum(in_file); bindex++) {
		hidden_file = hrfs_f2branch(in_file, bindex);

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
EXPORT_SYMBOL(hrfs_file_sendfile);
#else
ssize_t hrfs_file_sendfile(struct file *in_file, loff_t *ppos,
                                size_t len, read_actor_t actor, void *target)
{
	ssize_t err = 0;
	HENTRY();

	if (hrfs_f2info(in_file) != NULL) { 
		err = ll_hrfs_file_sendfile(in_file, hrfs_f2bnum(in_file), hrfs_f2barray(in_file), ppos, len, actor, target);
	}

	HRETURN(err);
}
#endif

struct file_operations hrfs_dir_fops =
{
	llseek:   hrfs_file_llseek,
	read:     generic_read_dir, /* always returns error */
	readdir:  hrfs_readdir,
	ioctl:    hrfs_ioctl,
	open:     hrfs_open,
	release:  hrfs_release,
	flush:    hrfs_flush,
};

struct file_operations hrfs_main_fops =
{
	llseek:     hrfs_file_llseek,
	read:       hrfs_file_read,
	aio_read:   hrfs_file_aio_read,
	write:      hrfs_file_write,
	aio_write:  hrfs_file_aio_write,
	sendfile:   hrfs_file_sendfile, 
	readv:      hrfs_file_readv,
	writev:     hrfs_file_writev,
	readdir:    hrfs_readdir,
	poll:       hrfs_poll,
	ioctl:      hrfs_ioctl,
	mmap:       hrfs_file_mmap,
	open:       hrfs_open,
//	flush:      hrfs_flush,
	release:    hrfs_release,
	fsync:      hrfs_fsync,
	fasync:     hrfs_fasync,
	lock:       hrfs_lock,
	flock:      hrfs_flock,
	/* not implemented: sendpage */
	/* not implemented: get_unmapped_area */
};
