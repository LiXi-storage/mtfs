/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <linux/module.h>
#include <mtfs_heal.h>
#include <mtfs_lowerfs.h>
#include <mtfs_super.h>
#include <mtfs_dentry.h>
#include <mtfs_inode.h>
#include <mtfs_file.h>
#include <mtfs_junction.h>
#include "async_replica_ext2.h"

struct super_operations mtfs_ext_sops =
{
	alloc_inode:    mtfs_alloc_inode,
	destroy_inode:  mtfs_destroy_inode,
	drop_inode:     generic_delete_inode,
	put_super:      mtfs_put_super,
	statfs:         mtfs_statfs,
	clear_inode:    mtfs_clear_inode,
	show_options:   mtfs_show_options,
};

struct inode_operations mtfs_ext_symlink_iops =
{
	readlink:       mtfs_readlink,
	follow_link:    mtfs_follow_link,
	put_link:       mtfs_put_link,
	permission:     mtfs_permission,
	setattr:        mtfs_setattr,
	getattr:        mtfs_getattr,
};

struct inode_operations mtfs_ext_dir_iops =
{
	create:	        mtfs_create,
	lookup:	        mtfs_lookup_nonnd,
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

struct inode_operations mtfs_ext_main_iops =
{
	permission:     mtfs_permission,
	setattr:        mtfs_setattr,
	getattr:        mtfs_getattr,
	setxattr:       mtfs_setxattr,
	getxattr:       mtfs_getxattr,
	removexattr:    mtfs_removexattr,
	listxattr:      mtfs_listxattr,
};

int mtfs_d_revalidate_local(struct dentry *dentry, struct nameidata *nd)
{
	int ret = 0;
	MENTRY();

	MDEBUG("d_revalidate [%.*s]\n", dentry->d_name.len, dentry->d_name.name);

	if (dentry->d_flags & DCACHE_MTFS_INVALID) {
		ret = 0;
		goto out;
	}
	ret = 1;
out:
	MRETURN(ret);
}

struct dentry_operations mtfs_ext_dops = {
	d_revalidate:  mtfs_d_revalidate_local,
	d_release:     mtfs_d_release,
};

struct file_operations mtfs_ext_dir_fops =
{
	llseek:   mtfs_file_llseek,
	read:     generic_read_dir,
	readdir:  mtfs_readdir,
	ioctl:    mtfs_ioctl,
	open:     mtfs_open,
	release:  mtfs_release,
	/* TODO: fsync, do we really need open? */
};

struct file_operations mtfs_ext_main_fops =
{
	llseek:     mtfs_file_llseek,
	read:       mtfs_file_read,
	write:      mtfs_file_write,
#ifdef HAVE_KERNEL_SENDFILE
	sendfile:   mtfs_file_sendfile,
#endif /* HAVE_KERNEL_SENDFILE */
#ifdef HAVE_FILE_READV
	readv:      masync_file_readv,
#else /* !HAVE_FILE_READV */
	aio_read:   masync_file_aio_read,
#endif /* !HAVE_FILE_READV */
#ifdef HAVE_FILE_WRITEV
	writev:     masync_file_writev,
#else /* !HAVE_FILE_WRITEV */
	aio_write:  masync_file_aio_write,
#endif /* !HAVE_FILE_WRITEV */
	readdir:    mtfs_readdir,
	poll:       mtfs_poll,
	ioctl:      mtfs_ioctl,
	mmap:       mtfs_file_mmap,
	open:       mtfs_open,
	release:    mtfs_release,
	fsync:      mtfs_fsync,
	/* TODO: splice_read, splice_write */
};

struct heal_operations mtfs_ext_hops =
{
	ho_discard_dentry: heal_discard_dentry_async,
};

static int mtfs_ext2_setflags(struct inode *inode, struct file *file, unsigned long arg)
{
	int ret = 0;
	int flags = 0;
	MENTRY();

	ret = mtfs_ioctl_write(inode, file, EXT2_IOC_SETFLAGS, arg, 0);
	if (ret < 0) {
		goto out;
	}

	ret = get_user(flags, (int *)arg);
	if (ret) {
		MERROR("failed to get_user, ret = %d\n", ret);
		goto out;
	}

	inode->i_flags = ext2_to_inode_flags(flags | EXT2_RESERVED_FL);
out:
	MRETURN(ret);
}

static int mtfs_ext2_getflags(struct inode *inode, struct file *file, unsigned long arg)
{
	int ret = 0;
	int flags = 0;
	MENTRY();

	ret = mtfs_ioctl_read(inode, file, EXT2_IOC_GETFLAGS, (unsigned long)&flags, 1);
	if (ret < 0) {
		goto out;
	}
	ret = put_user(flags, (int __user *)arg);
out:
	MRETURN(ret);
}

int mtfs_ext2_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	MENTRY();

	switch (cmd) {
	case EXT2_IOC_SETFLAGS:
		ret = mtfs_ext2_setflags(inode, file, arg);
		break;
	case EXT2_IOC_GETFLAGS:
		ret = mtfs_ext2_getflags(inode, file, arg);
		break;
	default:
		ret = -ENOTTY;
		break;
	}

	MRETURN(ret);
}

struct mtfs_operations mtfs_ext2_operations = {
	symlink_iops:            &mtfs_ext_symlink_iops,
	dir_iops:                &mtfs_ext_dir_iops,
	main_iops:               &mtfs_ext_main_iops,
	main_fops:               &mtfs_ext_main_fops,
	dir_fops:                &mtfs_ext_dir_fops,
	sops:                    &mtfs_ext_sops,
	dops:                    &mtfs_ext_dops,
	ioctl:                   &mtfs_ext2_ioctl,
	subject_ops:             &masync_subject_ops,
};

const char *ext2_supported_secondary_types[] = {
	"ext2",
	NULL,
};

struct mtfs_junction mtfs_ext2_junction = {
	mj_owner:                THIS_MODULE,
	mj_name:                 "ext2",
	mj_subject:              "ASYNC_REPLICA",
	mj_primary_type:         "ext2",
	mj_secondary_types:      ext2_supported_secondary_types,
	mj_fs_ops:              &mtfs_ext2_operations,
};

static int async_replica_ext_init(void)
{
	int ret = 0;

	MDEBUG("registering mtfs_ext2 support\n");


	ret = junction_register(&mtfs_ext2_junction);
	if (ret) {
		MERROR("failed to register junction: error %d\n", ret);
	}

	return ret;
}

static void async_replica_ext_exit(void)
{
	MDEBUG("unregistering mtfs_ext2 support\n");
	junction_unregister(&mtfs_ext2_junction);
}

MODULE_AUTHOR("MulTi File System Workgroup");
MODULE_DESCRIPTION("mtfs's async support for ext");
MODULE_LICENSE("GPL");

module_init(async_replica_ext_init);
module_exit(async_replica_ext_exit);
