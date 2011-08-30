/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
 */

#include <hrfs_support.h>
#include <hrfs_super.h>
#include <hrfs_dentry.h>
#include <hrfs_inode.h>
#include <hrfs_mmap.h>
#include <hrfs_stack.h>
#include <hrfs_junction.h>
#include "tmpfs_support.h"

size_t hrfs_tmpfs_file_read_branch(struct file *file, char __user *buf, size_t len,
                                   loff_t *ppos, hrfs_bindex_t bindex)
{
	ssize_t ret = 0;
	struct file *hidden_file = hrfs_f2branch(file, bindex);
	struct inode *inode = NULL;
	struct inode *hidden_inode = NULL;
	HENTRY();

	if (hidden_file) {
		HASSERT(hidden_file->f_op);
		HASSERT(hidden_file->f_op->read);
		ret = hidden_file->f_op->read(hidden_file, buf, len, ppos);
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

ssize_t hrfs_tmpfs_file_read(struct file *file, char __user *buf, size_t len,
                       loff_t *ppos)
{
	ssize_t ret = 0;
	loff_t tmp_pos = 0;
	hrfs_bindex_t bindex = 0;
	HENTRY();

	for (bindex = 0; bindex < hrfs_f2bnum(file); bindex++) {
		tmp_pos = *ppos;
		ret = hrfs_tmpfs_file_read_branch(file, buf, len, &tmp_pos, bindex);
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

static ssize_t hrfs_tmpfs_file_write_branch(struct file *file, const char __user *buf, size_t len, 
                                      loff_t *ppos , hrfs_bindex_t bindex)
{
	ssize_t ret = 0;
	struct file *hidden_file = hrfs_f2branch(file, bindex);
	struct inode *inode = NULL;
	struct inode *hidden_inode = NULL;
	HENTRY();

	if (hidden_file) {
		HASSERT(hidden_file->f_op);
		HASSERT(hidden_file->f_op->write);
		ret = hidden_file->f_op->write(hidden_file, buf, len, ppos);
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

ssize_t hrfs_tmpfs_file_write(struct file *file, const char __user *buf, size_t len, loff_t *ppos)
{
	ssize_t ret = 0;
	loff_t tmp_pos = 0;
	hrfs_bindex_t bindex = 0;
	loff_t success_pos = 0;
	ssize_t success_ret = 0;
	HENTRY();

	for (bindex = 0; bindex < hrfs_f2bnum(file); bindex++) {
		tmp_pos = *ppos;
		ret = hrfs_tmpfs_file_write_branch(file, buf, len, &tmp_pos, bindex);
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

struct dentry *hrfs_tmpfs_lookup(struct inode *dir, struct dentry *dentry, struct nameidata *nd)
{
	struct dentry *ret = NULL;
	int rc = 0;
	HENTRY();

	HASSERT(inode_is_locked(dir));
	HASSERT(!IS_ROOT(dentry));

	rc = hrfs_lookup_backend(dir, dentry, INTERPOSE_LOOKUP);

	ret = ERR_PTR(rc);

	HRETURN(ret);
}

struct super_operations hrfs_tmpfs_sops =
{
	alloc_inode:    hrfs_alloc_inode,
	destroy_inode:  hrfs_destroy_inode,
	drop_inode:     generic_delete_inode,
	put_super:      hrfs_put_super,
	statfs:         hrfs_statfs,
	clear_inode:    hrfs_clear_inode,
	show_options:   hrfs_show_options,
};

struct inode_operations hrfs_tmpfs_symlink_iops =
{
	readlink:       hrfs_readlink,
	follow_link:    hrfs_follow_link,
	permission:     hrfs_permission,
	setattr:        hrfs_setattr,
	getattr:        hrfs_getattr,
};

struct inode_operations hrfs_tmpfs_dir_iops =
{
	create:	        hrfs_create,
	lookup:	        hrfs_tmpfs_lookup,
	link:           hrfs_link,
	unlink:	        hrfs_unlink,
	symlink:        hrfs_symlink,
	mkdir:	        hrfs_mkdir,
	rmdir:	        hrfs_rmdir,
	mknod:	        hrfs_mknod,
	rename:	        hrfs_rename,
	permission:     hrfs_permission,
	setattr:        hrfs_setattr,
	getattr:        hrfs_getattr,
};

struct inode_operations hrfs_tmpfs_main_iops =
{
	permission:     hrfs_permission,
	setattr:        hrfs_setattr,
	getattr:        hrfs_getattr,
};

struct dentry_operations hrfs_tmpfs_dops = {
	d_release:     hrfs_d_release,
};

struct file_operations hrfs_tmpfs_dir_fops =
{
	llseek:   hrfs_file_llseek,
	read:     generic_read_dir,
	readdir:  hrfs_readdir,
	ioctl:    hrfs_ioctl,
	open:     hrfs_open,
	release:  hrfs_release,
	/* TODO: fsync, do we really need open? */
};

struct file_operations hrfs_tmpfs_main_fops =
{
	llseek:     hrfs_file_llseek,
	read:       hrfs_tmpfs_file_read,
	write:      hrfs_tmpfs_file_write,
	sendfile:   hrfs_file_sendfile, 
	readdir:    hrfs_readdir,
	ioctl:      hrfs_ioctl,
	mmap:       hrfs_file_mmap,
	open:       hrfs_open,
	release:    hrfs_release,
	fsync:      hrfs_fsync,
	/* TODO: splice_read, splice_write */
};

struct address_space_operations hrfs_tmpfs_aops =
{
	writepage:      hrfs_writepage,
	readpage:       hrfs_readpage_1,
	prepare_write:  hrfs_prepare_write,  /* Never needed */
	commit_write:   hrfs_commit_write    /* Never needed */
};

struct hrfs_operations hrfs_tmpfs_operations = {
	symlink_iops:            &hrfs_tmpfs_symlink_iops,
	dir_iops:                &hrfs_tmpfs_dir_iops,
	main_iops:               &hrfs_tmpfs_main_iops,
	main_fops:               &hrfs_tmpfs_main_fops,
	dir_fops:                &hrfs_tmpfs_dir_fops,
	sops:                    &hrfs_tmpfs_sops,
	dops:                    &hrfs_tmpfs_dops,
	aops:                    &hrfs_tmpfs_aops,
};

const char *supported_secondary_types[] = {
	"tmpfs",
	NULL,
};

struct hrfs_junction hrfs_tmpfs_junction = {
	junction_owner:          THIS_MODULE,
	junction_name:           "unknown",
	primary_type:            "tmpfs",
	secondary_types:         supported_secondary_types,
	fs_ops:                  &hrfs_tmpfs_operations,
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

	HDEBUG("registering hrfs_tmpfs support\n");

	ret = lowerfs_register_ops(&lowerfs_tmpfs_ops);
	if (ret) {
		HERROR("failed to register lowerfs operation: error %d\n", ret);
		goto out;
	}	

	ret = junction_register(&hrfs_tmpfs_junction);
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
	HDEBUG("unregistering hrfs_tmpfs support\n");
	lowerfs_unregister_ops(&lowerfs_tmpfs_ops);

	junction_unregister(&hrfs_tmpfs_junction);
}

MODULE_AUTHOR("Massive Storage Management Workgroup of Jiangnan Computing Technology Institute");
MODULE_DESCRIPTION("hrfs's support for tmpfs");
MODULE_LICENSE("GPL");

module_init(tmpfs_support_init);
module_exit(tmpfs_support_exit);
