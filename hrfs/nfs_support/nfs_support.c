/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
 */

#include <linux/magic.h>

#include <hrfs_support.h>
#include <hrfs_super.h>
#include <hrfs_dentry.h>
#include <hrfs_inode.h>
#include <hrfs_file.h>
#include <hrfs_junction.h>
#include "nfs_support.h"

struct dentry *hrfs_nfs_lookup(struct inode *dir, struct dentry *dentry, struct nameidata *nd)
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

struct super_operations hrfs_nfs_sops =
{
	alloc_inode:    hrfs_alloc_inode,
	destroy_inode:  hrfs_destroy_inode,
	drop_inode:     generic_delete_inode,
	put_super:      hrfs_put_super,
	statfs:         hrfs_statfs,
	clear_inode:    hrfs_clear_inode,
	show_options:   hrfs_show_options,
};

struct inode_operations hrfs_nfs_symlink_iops =
{
	readlink:       hrfs_readlink,
	follow_link:    hrfs_follow_link,
	put_link:       hrfs_put_link,
	permission:     hrfs_permission,
	setattr:        hrfs_setattr,
	getattr:        hrfs_getattr,
};

struct inode_operations hrfs_nfs_dir_iops =
{
	create:	        hrfs_create,
	lookup:	        hrfs_nfs_lookup,
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

struct inode_operations hrfs_nfs_main_iops =
{
	permission:     hrfs_permission,
	setattr:        hrfs_setattr,
	getattr:        hrfs_getattr,
	setxattr:       hrfs_setxattr,
	getxattr:       hrfs_getxattr,
	removexattr:    hrfs_removexattr,
	listxattr:      hrfs_listxattr,
};

struct dentry_operations hrfs_nfs_dops = {
	d_release:     hrfs_d_release,
};

struct file_operations hrfs_nfs_dir_fops =
{
	llseek:   hrfs_file_llseek,
	read:     generic_read_dir,
	readdir:  hrfs_readdir,
	ioctl:    hrfs_ioctl,
	open:     hrfs_open,
	release:  hrfs_release,
	/* TODO: fsync, do we really need open? */
};

struct file_operations hrfs_nfs_main_fops =
{
	llseek:     hrfs_file_llseek,
	read:       hrfs_file_read_nonreadv,
	aio_read:   hrfs_file_aio_read,
	write:      hrfs_file_write_nonwritev,
	aio_write:  hrfs_file_aio_write,
	sendfile:   hrfs_file_sendfile, 
	readdir:    hrfs_readdir,
	poll:       hrfs_poll,
	ioctl:      hrfs_ioctl,
	mmap:       hrfs_file_mmap,
	open:       hrfs_open,
	release:    hrfs_release,
	fsync:      hrfs_fsync,
	/* TODO: splice_read, splice_write */
};

int hrfs_nfs_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	HENTRY();

	switch (cmd) {
	default:
		ret = -ENOTTY;
		break;
	}

	HRETURN(ret);
}

struct hrfs_operations hrfs_nfs_operations = {
	symlink_iops:            &hrfs_nfs_symlink_iops,
	dir_iops:                &hrfs_nfs_dir_iops,
	main_iops:               &hrfs_nfs_main_iops,
	main_fops:               &hrfs_nfs_main_fops,
	dir_fops:                &hrfs_nfs_dir_fops,
	sops:                    &hrfs_nfs_sops,
	dops:                    &hrfs_nfs_dops,
	ioctl:                   &hrfs_nfs_ioctl,
};

const char *supported_secondary_types[] = {
	"nfs",
	NULL,
};

struct hrfs_junction hrfs_nfs_junction = {
	junction_owner:          THIS_MODULE,
	junction_name:           "unknown",
	primary_type:            "nfs",
	secondary_types:         supported_secondary_types,
	fs_ops:                  &hrfs_nfs_operations,
};

struct lowerfs_operations lowerfs_nfs_ops = {
	lowerfs_owner:           THIS_MODULE,
	lowerfs_type:            "nfs",
	lowerfs_magic:           NFS_SUPER_MAGIC,
	lowerfs_flag:            0,
	lowerfs_inode_set_flag:  NULL,
	lowerfs_inode_get_flag:  NULL,
	lowerfs_idata_init:      NULL,
	lowerfs_idata_finit:     NULL,
};

static int nfs_support_init(void)
{
	int ret = 0;

	HDEBUG("registering hrfs_nfs support\n");

	ret = lowerfs_register_ops(&lowerfs_nfs_ops);
	if (ret) {
		HERROR("failed to register lowerfs operation: error %d\n", ret);
		goto out;
	}	

	ret = junction_register(&hrfs_nfs_junction);
	if (ret) {
		HERROR("failed to register junction: error %d\n", ret);
		goto out_unregister_lowerfs_ops;
	}
	goto out;
out_unregister_lowerfs_ops:
	lowerfs_unregister_ops(&lowerfs_nfs_ops);
out:
	return ret;
}

static void nfs_support_exit(void)
{
	HDEBUG("unregistering hrfs_nfs support\n");
	lowerfs_unregister_ops(&lowerfs_nfs_ops);

	junction_unregister(&hrfs_nfs_junction);
}

MODULE_AUTHOR("Massive Storage Management Workgroup of Jiangnan Computing Technology Institute");
MODULE_DESCRIPTION("hrfs's support for nfs");
MODULE_LICENSE("GPL");

module_init(nfs_support_init);
module_exit(nfs_support_exit);
