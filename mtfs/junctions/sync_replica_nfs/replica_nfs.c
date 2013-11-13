/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <mtfs_lowerfs.h>
#include <mtfs_super.h>
#include <mtfs_dentry.h>
#include <mtfs_inode.h>
#include <mtfs_file.h>
#include <mtfs_mmap.h>
#include <mtfs_junction.h>
#include <linux/module.h>

static struct dentry *mtfs_nfs_lookup(struct inode *dir, struct dentry *dentry, struct nameidata *nd)
{
	struct dentry *ret = NULL;
	int rc = 0;
	MENTRY();

	MASSERT(inode_is_locked(dir));
	MASSERT(!IS_ROOT(dentry));

	rc = mtfs_lookup_backend(dir, dentry, INTERPOSE_LOOKUP);

	ret = ERR_PTR(rc);

	MRETURN(ret);
}

static struct super_operations mtfs_nfs_sops =
{
	alloc_inode:    mtfs_alloc_inode,
	destroy_inode:  mtfs_destroy_inode,
	drop_inode:     generic_delete_inode,
	put_super:      mtfs_put_super,
	statfs:         mtfs_statfs,
	clear_inode:    mtfs_clear_inode,
	show_options:   mtfs_show_options,
};

static struct inode_operations mtfs_nfs_symlink_iops =
{
	readlink:       mtfs_readlink,
	follow_link:    mtfs_follow_link,
	put_link:       mtfs_put_link,
	permission:     mtfs_permission,
	setattr:        mtfs_setattr,
	getattr:        mtfs_getattr,
};

static struct inode_operations mtfs_nfs_dir_iops =
{
	create:	        mtfs_create,
	lookup:	        mtfs_nfs_lookup,
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

static struct inode_operations mtfs_nfs_main_iops =
{
	permission:     mtfs_permission,
	setattr:        mtfs_setattr,
	getattr:        mtfs_getattr,
	setxattr:       mtfs_setxattr,
	getxattr:       mtfs_getxattr,
	removexattr:    mtfs_removexattr,
	listxattr:      mtfs_listxattr,
};

static struct dentry_operations mtfs_nfs_dops = {
	d_revalidate:   mtfs_d_revalidate,
	d_release:      mtfs_d_release,
};

static struct file_operations mtfs_nfs_dir_fops =
{
	llseek:         mtfs_file_llseek,
	read:           generic_read_dir,
	readdir:        mtfs_readdir,
	ioctl:          mtfs_ioctl,
	open:           mtfs_open,
	release:        mtfs_release,
	/* TODO: fsync, do we really need open? */
};

static struct file_operations mtfs_nfs_main_fops =
{
	llseek:         mtfs_file_llseek,
	read:           mtfs_file_read_nonreadv,
	write:          mtfs_file_write_nonwritev,
#ifdef HAVE_KERNEL_SENDFILE
	sendfile:       mtfs_file_sendfile,
#endif /* HAVE_KERNEL_SENDFILE */
	readdir:        mtfs_readdir,
	poll:           mtfs_poll,
	ioctl:          mtfs_ioctl,
	mmap:           mtfs_file_mmap,
	open:           mtfs_open,
	release:        mtfs_release,
	fsync:          mtfs_fsync,
	/* TODO: splice_read, splice_write */
};

static struct address_space_operations mtfs_nfs_aops =
{
	direct_IO:      mtfs_direct_IO,
	writepage:      mtfs_writepage,
	readpage:       mtfs_readpage,
};

static int mtfs_nfs_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	MENTRY();

	switch (cmd) {
	default:
		ret = -ENOTTY;
		break;
	}

	MRETURN(ret);
}

static struct mtfs_operations mtfs_nfs_operations = {
	symlink_iops:            &mtfs_nfs_symlink_iops,
	dir_iops:                &mtfs_nfs_dir_iops,
	main_iops:               &mtfs_nfs_main_iops,
	main_fops:               &mtfs_nfs_main_fops,
	dir_fops:                &mtfs_nfs_dir_fops,
	sops:                    &mtfs_nfs_sops,
	dops:                    &mtfs_nfs_dops,
	aops:                    &mtfs_nfs_aops,
	ioctl:                   &mtfs_nfs_ioctl,
	vm_ops:                  &mtfs_file_vm_ops,
	iupdate_ops:             &mtfs_iupdate_choose,
	io_ops:                  &mtfs_io_ops,
};

static const char *supported_secondary_types[] = {
	"nfs",
	NULL,
};

static struct mtfs_junction mtfs_nfs_junction = {
	mj_owner:                THIS_MODULE,
	mj_name:                 "unknown",
	mj_subject:              "SYNC_REPLICA",
	mj_primary_type:         "nfs",
	mj_secondary_types:      supported_secondary_types,
	mj_fs_ops:               &mtfs_nfs_operations,
};

static int nfs_support_init(void)
{
	int ret = 0;

	MDEBUG("registering sync replica junction for nfs\n");

	ret = junction_register(&mtfs_nfs_junction);
	if (ret) {
		MERROR("failed to register junction: ret = %d\n", ret);
	}

	return ret;
}

static void nfs_support_exit(void)
{
	MDEBUG("unregistering sync replica junction for nfs\n");

	junction_unregister(&mtfs_nfs_junction);
}

MODULE_AUTHOR("MulTi File System Workgroup");
MODULE_DESCRIPTION("mtfs's sync replica junction for nfs");
MODULE_LICENSE("GPL");

module_init(nfs_support_init);
module_exit(nfs_support_exit);
