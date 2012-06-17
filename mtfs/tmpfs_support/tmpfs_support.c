/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <mtfs_support.h>
#include <mtfs_super.h>
#include <mtfs_dentry.h>
#include <mtfs_inode.h>
#include <mtfs_mmap.h>
#include <mtfs_stack.h>
#include <mtfs_junction.h>
#include <linux/module.h>
#include "tmpfs_support.h"

#define DEBUG_SUBSYSTEM S_MTFS

size_t mtfs_tmpfs_file_read_branch(struct file *file, char __user *buf, size_t len,
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

ssize_t mtfs_tmpfs_file_read(struct file *file, char __user *buf, size_t len,
                       loff_t *ppos)
{
	ssize_t ret = 0;
	loff_t tmp_pos = 0;
	mtfs_bindex_t bindex = 0;
	HENTRY();

	for (bindex = 0; bindex < mtfs_f2bnum(file); bindex++) {
		tmp_pos = *ppos;
		ret = mtfs_tmpfs_file_read_branch(file, buf, len, &tmp_pos, bindex);
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

static ssize_t mtfs_tmpfs_file_write_branch(struct file *file, const char __user *buf, size_t len, 
                                      loff_t *ppos , mtfs_bindex_t bindex)
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

ssize_t mtfs_tmpfs_file_write(struct file *file, const char __user *buf, size_t len, loff_t *ppos)
{
	ssize_t ret = 0;
	loff_t tmp_pos = 0;
	mtfs_bindex_t bindex = 0;
	loff_t success_pos = 0;
	ssize_t success_ret = 0;
	HENTRY();

	for (bindex = 0; bindex < mtfs_f2bnum(file); bindex++) {
		tmp_pos = *ppos;
		ret = mtfs_tmpfs_file_write_branch(file, buf, len, &tmp_pos, bindex);
		HDEBUG("readed branch[%d] of file [%*s] at pos = %llu, ret = %ld\n",
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

struct dentry *mtfs_tmpfs_lookup(struct inode *dir, struct dentry *dentry, struct nameidata *nd)
{
	struct dentry *ret = NULL;
	int rc = 0;
	HENTRY();

	HASSERT(inode_is_locked(dir));
	HASSERT(!IS_ROOT(dentry));

	rc = mtfs_lookup_backend(dir, dentry, INTERPOSE_LOOKUP);

	ret = ERR_PTR(rc);

	HRETURN(ret);
}

struct super_operations mtfs_tmpfs_sops =
{
	alloc_inode:    mtfs_alloc_inode,
	destroy_inode:  mtfs_destroy_inode,
	drop_inode:     generic_delete_inode,
	put_super:      mtfs_put_super,
	statfs:         mtfs_statfs,
	clear_inode:    mtfs_clear_inode,
	show_options:   mtfs_show_options,
};

struct inode_operations mtfs_tmpfs_symlink_iops =
{
	readlink:       mtfs_readlink,
	follow_link:    mtfs_follow_link,
	permission:     mtfs_permission,
	setattr:        mtfs_setattr,
	getattr:        mtfs_getattr,
};

struct inode_operations mtfs_tmpfs_dir_iops =
{
	create:	        mtfs_create,
	lookup:	        mtfs_tmpfs_lookup,
	link:           mtfs_link,
	unlink:	        mtfs_unlink,
	symlink:        mtfs_symlink,
	mkdir:	        mtfs_mkdir,
	rmdir:	        mtfs_rmdir,
	mknod:	        mtfs_mknod,
	rename:	        mtfs_rename,
	permission:     mtfs_permission,
	setattr:        mtfs_setattr,
	getattr:        mtfs_getattr,
};

struct inode_operations mtfs_tmpfs_main_iops =
{
	permission:     mtfs_permission,
	setattr:        mtfs_setattr,
	getattr:        mtfs_getattr,
};

struct dentry_operations mtfs_tmpfs_dops = {
	d_release:     mtfs_d_release,
};

struct file_operations mtfs_tmpfs_dir_fops =
{
	llseek:   mtfs_file_llseek,
	read:     generic_read_dir,
	readdir:  mtfs_readdir,
	ioctl:    mtfs_ioctl,
	open:     mtfs_open,
	release:  mtfs_release,
	/* TODO: fsync, do we really need open? */
};

struct file_operations mtfs_tmpfs_main_fops =
{
	llseek:     mtfs_file_llseek,
	read:       mtfs_tmpfs_file_read,
	write:      mtfs_tmpfs_file_write,
#ifdef HAVE_KERNEL_SENDFILE
	sendfile:   mtfs_file_sendfile,
#endif /* HAVE_KERNEL_SENDFILE */
	readdir:    mtfs_readdir,
	ioctl:      mtfs_ioctl,
	mmap:       mtfs_file_mmap,
	open:       mtfs_open,
	release:    mtfs_release,
	fsync:      mtfs_fsync,
	/* TODO: splice_read, splice_write */
};

struct address_space_operations mtfs_tmpfs_aops =
{
	writepage:      mtfs_writepage,
	readpage:       mtfs_readpage,
};

struct mtfs_operations mtfs_tmpfs_operations = {
	symlink_iops:            &mtfs_tmpfs_symlink_iops,
	dir_iops:                &mtfs_tmpfs_dir_iops,
	main_iops:               &mtfs_tmpfs_main_iops,
	main_fops:               &mtfs_tmpfs_main_fops,
	dir_fops:                &mtfs_tmpfs_dir_fops,
	sops:                    &mtfs_tmpfs_sops,
	dops:                    &mtfs_tmpfs_dops,
	aops:                    &mtfs_tmpfs_aops,
};

const char *supported_secondary_types[] = {
	"tmpfs",
	NULL,
};

struct mtfs_junction mtfs_tmpfs_junction = {
	junction_owner:          THIS_MODULE,
	junction_name:           "unknown",
	primary_type:            "tmpfs",
	secondary_types:         supported_secondary_types,
	fs_ops:                  &mtfs_tmpfs_operations,
};

struct lowerfs_operations lowerfs_tmpfs_ops = {
	lowerfs_owner:           THIS_MODULE,
	lowerfs_type:            "tmpfs",
	lowerfs_magic:           TMPFS_MAGIC,
	lowerfs_flag:            0,
	lowerfs_inode_set_flag:  NULL,
	lowerfs_inode_get_flag:  NULL,
	lowerfs_idata_init:      NULL,
	lowerfs_idata_finit:     NULL,
};

static int tmpfs_support_init(void)
{
	int ret = 0;

	HDEBUG("registering mtfs_tmpfs support\n");

	ret = lowerfs_register_ops(&lowerfs_tmpfs_ops);
	if (ret) {
		HERROR("failed to register lowerfs operation: error %d\n", ret);
		goto out;
	}	

	ret = junction_register(&mtfs_tmpfs_junction);
	if (ret) {
		HERROR("failed to register junction: error %d\n", ret);
		goto out_unregister_lowerfs_ops;
	}
	goto out;
out_unregister_lowerfs_ops:
	lowerfs_unregister_ops(&lowerfs_tmpfs_ops);
out:
	return ret;
}

static void tmpfs_support_exit(void)
{
	HDEBUG("unregistering mtfs_tmpfs support\n");
	lowerfs_unregister_ops(&lowerfs_tmpfs_ops);

	junction_unregister(&mtfs_tmpfs_junction);
}

MODULE_AUTHOR("Massive Storage Management Workgroup of Jiangnan Computing Technology Institute");
MODULE_DESCRIPTION("mtfs's support for tmpfs");
MODULE_LICENSE("GPL");

module_init(tmpfs_support_init);
module_exit(tmpfs_support_exit);
