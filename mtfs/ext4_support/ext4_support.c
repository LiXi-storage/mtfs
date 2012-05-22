/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <mtfs_support.h>
#include <mtfs_super.h>
#include <mtfs_dentry.h>
#include <mtfs_inode.h>
#include <mtfs_junction.h>
#include "ext4_support.h"

struct dentry *mtfs_ext4_lookup(struct inode *dir, struct dentry *dentry, struct nameidata *nd)
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

struct super_operations mtfs_ext4_sops =
{
	alloc_inode:    mtfs_alloc_inode,
	destroy_inode:  mtfs_destroy_inode,
	drop_inode:     generic_delete_inode,
	put_super:      mtfs_put_super,
	statfs:         mtfs_statfs,
	clear_inode:    mtfs_clear_inode,
	show_options:   mtfs_show_options,
};

struct inode_operations mtfs_ext4_symlink_iops =
{
	readlink:       mtfs_readlink,
	follow_link:    mtfs_follow_link,
	put_link:       mtfs_put_link,
	permission:     mtfs_permission,
	setattr:        mtfs_setattr,
	getattr:        mtfs_getattr,
};

struct inode_operations mtfs_ext4_dir_iops =
{
	create:	        mtfs_create,
	lookup:	        mtfs_ext4_lookup,
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
	setxattr:       mtfs_setxattr,
	getxattr:       mtfs_getxattr,
	removexattr:    mtfs_removexattr,
	listxattr:      mtfs_listxattr,
};

struct inode_operations mtfs_ext4_main_iops =
{
	permission:     mtfs_permission,
	setattr:        mtfs_setattr,
	getattr:        mtfs_getattr,
	setxattr:       mtfs_setxattr,
	getxattr:       mtfs_getxattr,
	removexattr:    mtfs_removexattr,
	listxattr:      mtfs_listxattr,
};

struct dentry_operations mtfs_ext4_dops = {
	d_release:     mtfs_d_release,
};

struct file_operations mtfs_ext4_dir_fops =
{
	llseek:   mtfs_file_llseek,
	read:     generic_read_dir,
	readdir:  mtfs_readdir,
	ioctl:    mtfs_ioctl,
	open:     mtfs_open,
	release:  mtfs_release,
	/* TODO: fsync, do we really need open? */
};

struct file_operations mtfs_ext4_main_fops =
{
	llseek:     mtfs_file_llseek,
	read:       mtfs_file_read,
	aio_read:   mtfs_file_aio_read,
	write:      mtfs_file_write,
	aio_write:  mtfs_file_aio_write,
	sendfile:   mtfs_file_sendfile, 
	readv:      mtfs_file_readv,
	writev:     mtfs_file_writev,
	readdir:    mtfs_readdir,
	poll:       mtfs_poll,
	ioctl:      mtfs_ioctl,
	mmap:       mtfs_file_mmap,
	open:       mtfs_open,
	release:    mtfs_release,
	fsync:      mtfs_fsync,
	/* TODO: splice_read, splice_write */
};

struct mtfs_operations mtfs_ext4_operations = {
	symlink_iops:            &mtfs_ext4_symlink_iops,
	dir_iops:                &mtfs_ext4_dir_iops,
	main_iops:               &mtfs_ext4_main_iops,
	main_fops:               &mtfs_ext4_main_fops,
	dir_fops:                &mtfs_ext4_dir_fops,
	sops:                    &mtfs_ext4_sops,
	dops:                    &mtfs_ext4_dops,
};

const char *supported_secondary_types[] = {
	"ext4",
	NULL,
};

struct mtfs_junction mtfs_ext4_junction = {
	junction_owner:          THIS_MODULE,
	junction_name:           "unknown",
	primary_type:            "ext4",
	secondary_types:         supported_secondary_types,
	fs_ops:                  &mtfs_ext4_operations,
};

struct lowerfs_operations lowerfs_ext4_ops = {
	lowerfs_owner:           THIS_MODULE,
	lowerfs_type:            "ext4",
	lowerfs_magic:           EXT4_SUPER_MAGIC,
	lowerfs_flag:            0,
	lowerfs_inode_set_flag:  NULL,
	lowerfs_inode_get_flag:  NULL,
	lowerfs_idata_init:      NULL,
	lowerfs_idata_finit:     NULL,
};

static int ext4_support_init(void)
{
	int ret = 0;

	HDEBUG("registering mtfs_ext4 support\n");

	ret = lowerfs_register_ops(&lowerfs_ext4_ops);
	if (ret) {
		HERROR("failed to register lowerfs operation: error %d\n", ret);
		goto out;
	}	

	ret = junction_register(&mtfs_ext4_junction);
	if (ret) {
		HERROR("failed to register junction: error %d\n", ret);
		goto out_unregister_lowerfs_ops;
	}
	goto out;
out_unregister_lowerfs_ops:
	lowerfs_unregister_ops(&lowerfs_ext4_ops);
out:
	return ret;
}

static void ext4_support_exit(void)
{
	HDEBUG("unregistering mtfs_ext4 support\n");
	lowerfs_unregister_ops(&lowerfs_ext4_ops);

	junction_unregister(&mtfs_ext4_junction);
}

MODULE_AUTHOR("Massive Storage Management Workgroup of Jiangnan Computing Technology Institute");
MODULE_DESCRIPTION("mtfs's support for ext4");
MODULE_LICENSE("GPL");

module_init(ext4_support_init);
module_exit(ext4_support_exit);
