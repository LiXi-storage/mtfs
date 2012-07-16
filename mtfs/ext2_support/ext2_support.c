/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <mtfs_support.h>
#include <mtfs_super.h>
#include <mtfs_dentry.h>
#include <mtfs_inode.h>
#include <mtfs_file.h>
#include <mtfs_junction.h>
#include <linux/module.h>
#include "ext2_support.h"

struct dentry *mtfs_ext2_lookup(struct inode *dir, struct dentry *dentry, struct nameidata *nd)
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

struct super_operations mtfs_ext2_sops =
{
	alloc_inode:    mtfs_alloc_inode,
	destroy_inode:  mtfs_destroy_inode,
	drop_inode:     generic_delete_inode,
	put_super:      mtfs_put_super,
	statfs:         mtfs_statfs,
	clear_inode:    mtfs_clear_inode,
	show_options:   mtfs_show_options,
};

struct inode_operations mtfs_ext2_symlink_iops =
{
	readlink:       mtfs_readlink,
	follow_link:    mtfs_follow_link,
	put_link:       mtfs_put_link,
	permission:     mtfs_permission,
	setattr:        mtfs_setattr,
	getattr:        mtfs_getattr,
};

struct inode_operations mtfs_ext2_dir_iops =
{
	create:	        mtfs_create,
	lookup:	        mtfs_ext2_lookup,
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

struct inode_operations mtfs_ext2_main_iops =
{
	permission:     mtfs_permission,
	setattr:        mtfs_setattr,
	getattr:        mtfs_getattr,
	setxattr:       mtfs_setxattr,
	getxattr:       mtfs_getxattr,
	removexattr:    mtfs_removexattr,
	listxattr:      mtfs_listxattr,
};

struct dentry_operations mtfs_ext2_dops = {
	d_release:     mtfs_d_release,
};

struct file_operations mtfs_ext2_dir_fops =
{
	llseek:   mtfs_file_llseek,
	read:     generic_read_dir,
	readdir:  mtfs_readdir,
	ioctl:    mtfs_ioctl,
	open:     mtfs_open,
	release:  mtfs_release,
	/* TODO: fsync, do we really need open? */
};

struct file_operations mtfs_ext2_main_fops =
{
	llseek:     mtfs_file_llseek,
	read:       mtfs_file_read,
	write:      mtfs_file_write,
#ifdef HAVE_KERNEL_SENDFILE
	sendfile:   mtfs_file_sendfile,
#endif /* HAVE_KERNEL_SENDFILE */
#ifdef HAVE_FILE_READV
	readv:      mtfs_file_readv,
#else /* !HAVE_FILE_READV */
	aio_read:   mtfs_file_aio_read,
#endif /* !HAVE_FILE_READV */
#ifdef HAVE_FILE_WRITEV
	writev:     mtfs_file_writev,
#else /* !HAVE_FILE_WRITEV */
	aio_write:  mtfs_file_aio_write,
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

static int mtfs_ext2_setflags(struct inode *inode, struct file *file, unsigned long arg)
{
	int ret = 0;
	int flags = 0;
	HENTRY();

	ret = mtfs_ioctl_write(inode, file, EXT2_IOC_SETFLAGS, arg, 0);
	if (ret < 0) {
		goto out;
	}

	ret = get_user(flags, (int *)arg);
	if (ret) {
		HERROR("failed to get_user, ret = %d\n", ret);
		goto out;
	}

	inode->i_flags = ext2_to_inode_flags(flags | EXT2_RESERVED_FL);
out:
	HRETURN(ret);
}

static int mtfs_ext2_getflags(struct inode *inode, struct file *file, unsigned long arg)
{
	int ret = 0;
	int flags = 0;
	HENTRY();

	ret = mtfs_ioctl_read(inode, file, EXT2_IOC_GETFLAGS, (unsigned long)&flags, 1);
	if (ret < 0) {
		goto out;
	}
	ret = put_user(flags, (int __user *)arg);
out:
	HRETURN(ret);
}

int mtfs_ext2_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	HENTRY();

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

	HRETURN(ret);
}

struct mtfs_operations mtfs_ext2_operations = {
	symlink_iops:            &mtfs_ext2_symlink_iops,
	dir_iops:                &mtfs_ext2_dir_iops,
	main_iops:               &mtfs_ext2_main_iops,
	main_fops:               &mtfs_ext2_main_fops,
	dir_fops:                &mtfs_ext2_dir_fops,
	sops:                    &mtfs_ext2_sops,
	dops:                    &mtfs_ext2_dops,
	ioctl:                   &mtfs_ext2_ioctl,
};

const char *supported_secondary_types[] = {
	"ext2",
	NULL,
};

struct mtfs_junction mtfs_ext2_junction = {
	junction_owner:          THIS_MODULE,
	junction_name:           "ext2",
	primary_type:            "ext2",
	secondary_types:         supported_secondary_types,
	fs_ops:                  &mtfs_ext2_operations,
};

#include <mtfs_flag.h>
struct lowerfs_operations lowerfs_ext2_ops = {
	lowerfs_owner:           THIS_MODULE,
	lowerfs_type:            "ext2",
	lowerfs_magic:           EXT2_SUPER_MAGIC,
	lowerfs_flag:            0,
	lowerfs_inode_set_flag:  lowerfs_inode_set_flag_default,
	lowerfs_inode_get_flag:  lowerfs_inode_get_flag_default,
	lowerfs_idata_init:      NULL,
	lowerfs_idata_finit:     NULL,
};

static int ext2_support_init(void)
{
	int ret = 0;

	HDEBUG("registering mtfs_ext2 support\n");

	ret = lowerfs_register_ops(&lowerfs_ext2_ops);
	if (ret) {
		HERROR("failed to register lowerfs operation: error %d\n", ret);
		goto out;
	}	

	ret = junction_register(&mtfs_ext2_junction);
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
	HDEBUG("unregistering mtfs_ext2 support\n");
	lowerfs_unregister_ops(&lowerfs_ext2_ops);

	junction_unregister(&mtfs_ext2_junction);
}

MODULE_AUTHOR("MulTi File System Workgroup");
MODULE_DESCRIPTION("mtfs's support for ext2");
MODULE_LICENSE("GPL");

module_init(ext2_support_init);
module_exit(ext2_support_exit);
