/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */
#include <linux/module.h>
#include <mtfs_super.h>
#include <mtfs_dentry.h>
#include <mtfs_inode.h>
#include <mtfs_file.h>
#include <mtfs_junction.h>
#include <mtfs_mmap.h>

struct dentry *mtfs_ntfs3g_lookup(struct inode *dir, struct dentry *dentry, struct nameidata *nd)
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

struct super_operations mtfs_ntfs3g_sops =
{
	alloc_inode:    mtfs_alloc_inode,
	destroy_inode:  mtfs_destroy_inode,
	drop_inode:     generic_delete_inode,
	put_super:      mtfs_put_super,
	statfs:         mtfs_statfs,
	clear_inode:    mtfs_clear_inode,
	show_options:   mtfs_show_options,
};

struct inode_operations mtfs_ntfs3g_symlink_iops =
{
	readlink:       mtfs_readlink,
	follow_link:    mtfs_follow_link,
	put_link:       mtfs_put_link,
	permission:     mtfs_permission,
	setattr:        mtfs_setattr,
	getattr:        mtfs_getattr,
};

struct inode_operations mtfs_ntfs3g_dir_iops =
{
	create:	        mtfs_create,
	lookup:	        mtfs_ntfs3g_lookup,
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

struct inode_operations mtfs_ntfs3g_main_iops =
{
	permission:     mtfs_permission,
	setattr:        mtfs_setattr,
	getattr:        mtfs_getattr,
	setxattr:       mtfs_setxattr,
	getxattr:       mtfs_getxattr,
	removexattr:    mtfs_removexattr,
	listxattr:      mtfs_listxattr,
};

struct dentry_operations mtfs_ntfs3g_dops = {
	d_release:      mtfs_d_release,
};

struct file_operations mtfs_ntfs3g_dir_fops =
{
	llseek:   mtfs_file_llseek,
	read:     generic_read_dir,
	readdir:  mtfs_readdir,
	ioctl:    mtfs_ioctl,
	open:     mtfs_open,
	release:  mtfs_release,
	/* TODO: fsync, do we really need open? */
};

struct file_operations mtfs_ntfs3g_main_fops =
{
	llseek:     mtfs_file_llseek,
	read:       mtfs_file_read_nonreadv,
	write:      mtfs_file_write_nonwritev,
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

int mtfs_ntfs3g_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
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

struct mtfs_operations mtfs_ntfs3g_operations = {
	symlink_iops:            &mtfs_ntfs3g_symlink_iops,
	dir_iops:                &mtfs_ntfs3g_dir_iops,
	main_iops:               &mtfs_ntfs3g_main_iops,
	main_fops:               &mtfs_ntfs3g_main_fops,
	dir_fops:                &mtfs_ntfs3g_dir_fops,
	sops:                    &mtfs_ntfs3g_sops,
	dops:                    &mtfs_ntfs3g_dops,
	aops:                    &mtfs_aops,
	ioctl:                   &mtfs_ntfs3g_ioctl,
	vm_ops:                  &mtfs_file_vm_ops,
	iupdate_ops:             &mtfs_iupdate_choose,
	io_ops:                  &mtfs_io_ops,
};

const char *supported_secondary_types[] = {
	"fuseblk",
	NULL,
};

struct mtfs_junction mtfs_ntfs3g_junction = {
	mj_owner:                THIS_MODULE,
	mj_name:                 "ntfs3g",
	mj_subject:              "SYNC_REPLICA",
	mj_primary_type:         "fuseblk",
	mj_secondary_types:      supported_secondary_types,
	mj_fs_ops:               &mtfs_ntfs3g_operations,
};


static int ntfs3g_support_init(void)
{
	int ret = 0;

	MDEBUG("registering sync replica junction for ntfs3g\n");

	ret = junction_register(&mtfs_ntfs3g_junction);
	if (ret) {
		MERROR("failed to register junction:  ret = %d\n", ret);
	}

	return ret;
}

static void ntfs3g_support_exit(void)
{
	MDEBUG("unregistering sync replica junction for ntfs3g\n");

	junction_unregister(&mtfs_ntfs3g_junction);
}

MODULE_AUTHOR("MulTi File System Workgroup");
MODULE_DESCRIPTION("mtfs's support for ntfs3g");
MODULE_LICENSE("GPL");

module_init(ntfs3g_support_init);
module_exit(ntfs3g_support_exit);

