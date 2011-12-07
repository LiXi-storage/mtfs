/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
 */

#include <hrfs_support.h>
#include <hrfs_super.h>
#include <hrfs_dentry.h>
#include <hrfs_inode.h>
#include <hrfs_file.h>
#include <linux/magic.h>
#include <hrfs_junction.h>
#include "ext2_support.h"

struct dentry *hrfs_ext2_lookup(struct inode *dir, struct dentry *dentry, struct nameidata *nd)
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

struct super_operations hrfs_ext2_sops =
{
	alloc_inode:    hrfs_alloc_inode,
	destroy_inode:  hrfs_destroy_inode,
	drop_inode:     generic_delete_inode,
	put_super:      hrfs_put_super,
	statfs:         hrfs_statfs,
	clear_inode:    hrfs_clear_inode,
	show_options:   hrfs_show_options,
};

struct inode_operations hrfs_ext2_symlink_iops =
{
	readlink:       hrfs_readlink,
	follow_link:    hrfs_follow_link,
	put_link:       hrfs_put_link,
	permission:     hrfs_permission,
	setattr:        hrfs_setattr,
	getattr:        hrfs_getattr,
};

struct inode_operations hrfs_ext2_dir_iops =
{
	create:	        hrfs_create,
	lookup:	        hrfs_ext2_lookup,
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
	setxattr:       hrfs_setxattr,
	getxattr:       hrfs_getxattr,
	removexattr:    hrfs_removexattr,
	listxattr:      hrfs_listxattr,
};

struct inode_operations hrfs_ext2_main_iops =
{
	permission:     hrfs_permission,
	setattr:        hrfs_setattr,
	getattr:        hrfs_getattr,
	setxattr:       hrfs_setxattr,
	getxattr:       hrfs_getxattr,
	removexattr:    hrfs_removexattr,
	listxattr:      hrfs_listxattr,
};

struct dentry_operations hrfs_ext2_dops = {
	d_release:     hrfs_d_release,
};

struct file_operations hrfs_ext2_dir_fops =
{
	llseek:   hrfs_file_llseek,
	read:     generic_read_dir,
	readdir:  hrfs_readdir,
	ioctl:    hrfs_ioctl,
	open:     hrfs_open,
	release:  hrfs_release,
	/* TODO: fsync, do we really need open? */
};

struct file_operations hrfs_ext2_main_fops =
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
	release:    hrfs_release,
	fsync:      hrfs_fsync,
	/* TODO: splice_read, splice_write */
};

static int hrfs_ext2_setflags(struct inode *inode, struct file *file, unsigned long arg)
{
	int ret = 0;
	struct file *hidden_file = NULL;
	struct inode *hidden_inode = NULL;
	hrfs_bindex_t bindex = 0;
	int undo_ret = 0;
	int flags = 0;
	mm_segment_t old_fs;
	HENTRY();

	HASSERT(hrfs_f2info(file));

	for(bindex = 0; bindex < hrfs_f2bnum(file); bindex++) {
		hidden_file = hrfs_f2branch(file, bindex);
		hidden_inode = hrfs_i2branch(inode, bindex);
		if (hidden_file == NULL || hidden_inode == NULL) {
			HERROR("branch[%d] of file [%*s] is NULL, ioctl setflags skipped\n", 
			       bindex, file->f_dentry->d_name.len, file->f_dentry->d_name.name);
			continue;
		}

		HASSERT(hidden_file->f_op);
		HASSERT(hidden_file->f_op->ioctl);
		ret = hidden_file->f_op->ioctl(hidden_inode, hidden_file, EXT2_IOC_SETFLAGS, arg);
		if (ret < 0) {
			HERROR("ioctl setflags branch[%d] of file [%*s] failed, ret = %d\n",
			       bindex, file->f_dentry->d_name.len, file->f_dentry->d_name.name, ret);
			bindex--;
			goto out_undo;
		}
	}

	if (get_user(flags, (int *)arg)) {
		ret = -EFAULT;
		goto out;
	}
	inode->i_flags = ext2_to_inode_flags(flags | EXT2_RESERVED_FL);
	goto out;
out_undo:
	for(; bindex >= 0; bindex--) {
		flags = inode_to_ext2_flags(inode->i_flags, 1);
		hidden_file = hrfs_f2branch(file, bindex);
		hidden_inode = hrfs_i2branch(inode, bindex);
		if (!hidden_file) {
			continue;
		}
		old_fs = get_fs();
		set_fs(get_ds());
		undo_ret = hidden_file->f_op->ioctl(hidden_inode, hidden_file, EXT2_IOC_SETFLAGS, (long)&flags);
		set_fs(old_fs);
		if (undo_ret < 0) {
			HERROR("undo ioctl setflags branch[%d] of file [%*s] failed, ret = %d\n",
			       bindex, file->f_dentry->d_name.len, file->f_dentry->d_name.name, undo_ret);
		}
	}
out:
	HRETURN(ret);
}

static int hrfs_ext2_getflags(struct inode *inode, struct file *file, unsigned long arg)
{
	int ret = 0;
	struct file *hidden_file = NULL;
	struct inode *hidden_inode = NULL;
	hrfs_bindex_t bindex = 0;
	int flags = 0;
	mm_segment_t old_fs;
	HENTRY();

	HASSERT(hrfs_f2info(file));
	for(bindex = 0; bindex < hrfs_f2bnum(file); bindex++) {
		hidden_file = hrfs_f2branch(file, bindex);
		hidden_inode = hrfs_i2branch(inode, bindex);
		if (hidden_file == NULL || hidden_inode == NULL) {
			HERROR("branch[%d] of file [%*s] is NULL, ioctl getflags skipped\n", 
			       bindex, file->f_dentry->d_name.len, file->f_dentry->d_name.name);
			continue;
		}

		HASSERT(hidden_file->f_op);
		HASSERT(hidden_file->f_op->ioctl);
		old_fs = get_fs();
		set_fs(get_ds());
		ret = hidden_file->f_op->ioctl(hidden_inode, hidden_file, EXT2_IOC_GETFLAGS, (long)&flags);
		set_fs(old_fs);
		//ret = hidden_file->f_op->ioctl(hidden_inode, hidden_file, EXT2_IOC_GETFLAGS, arg);
		if (ret < 0) {
			HERROR("ioctl getflags branch[%d] of file [%*s] failed, ret = %d\n",
			       bindex, file->f_dentry->d_name.len, file->f_dentry->d_name.name, ret);
		} else {
			inode->i_flags = ext2_to_inode_flags(flags | EXT2_RESERVED_FL);
			goto out;
		}
	}

out:
	if (ret < 0) {
		HRETURN(ret);
	} else {
		HRETURN(put_user(flags, (int __user *)arg));
	}
}

int hrfs_ext2_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	HENTRY();

	switch (cmd) {
	case EXT2_IOC_SETFLAGS:
		ret = hrfs_ext2_setflags(inode, file, arg);
		break;
	case EXT2_IOC_GETFLAGS:
		ret = hrfs_ext2_getflags(inode, file, arg);
		break;
	default:
		ret = -ENOTTY;
		break;
	}

	HRETURN(ret);
}

struct hrfs_operations hrfs_ext2_operations = {
	symlink_iops:            &hrfs_ext2_symlink_iops,
	dir_iops:                &hrfs_ext2_dir_iops,
	main_iops:               &hrfs_ext2_main_iops,
	main_fops:               &hrfs_ext2_main_fops,
	dir_fops:                &hrfs_ext2_dir_fops,
	sops:                    &hrfs_ext2_sops,
	dops:                    &hrfs_ext2_dops,
	ioctl:                   &hrfs_ext2_ioctl,
};

const char *supported_secondary_types[] = {
	"ext2",
	NULL,
};

struct hrfs_junction hrfs_ext2_junction = {
	junction_owner:          THIS_MODULE,
	junction_name:           "unknown",
	primary_type:            "ext2",
	secondary_types:         supported_secondary_types,
	fs_ops:                  &hrfs_ext2_operations,
};

struct lowerfs_operations lowerfs_ext2_ops = {
	lowerfs_owner:           THIS_MODULE,
	lowerfs_type:            "ext2",
	lowerfs_magic:           EXT2_SUPER_MAGIC,
	lowerfs_flag:            0,
	lowerfs_inode_set_flag:  NULL,
	lowerfs_inode_get_flag:  NULL,
	lowerfs_idata_init:      NULL,
	lowerfs_idata_finit:     NULL,
};

static int ext2_support_init(void)
{
	int ret = 0;

	HDEBUG("registering hrfs_ext2 support\n");

	ret = lowerfs_register_ops(&lowerfs_ext2_ops);
	if (ret) {
		HERROR("failed to register lowerfs operation: error %d\n", ret);
		goto out;
	}	

	ret = junction_register(&hrfs_ext2_junction);
	if (ret) {
		HERROR("failed to register junction: error %d\n", ret);
		goto out_unregister_lowerfs_ops;
	}
	goto out;
out_unregister_lowerfs_ops:
	lowerfs_unregister_ops(&lowerfs_ext2_ops);
out:
	return ret;
}

static void ext2_support_exit(void)
{
	HDEBUG("unregistering hrfs_ext2 support\n");
	lowerfs_unregister_ops(&lowerfs_ext2_ops);

	junction_unregister(&hrfs_ext2_junction);
}

MODULE_AUTHOR("Massive Storage Management Workgroup of Jiangnan Computing Technology Institute");
MODULE_DESCRIPTION("hrfs's support for ext2");
MODULE_LICENSE("GPL");

module_init(ext2_support_init);
module_exit(ext2_support_exit);
