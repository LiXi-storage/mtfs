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
#include <mtfs_mmap.h>
#include <mtfs_junction.h>
#include "replica_lustre.h"

struct super_operations mtfs_lustre_sops =
{
	alloc_inode:    mtfs_alloc_inode,
	destroy_inode:  mtfs_destroy_inode,
	drop_inode:     generic_delete_inode,
	put_super:      mtfs_put_super,
	statfs:         mtfs_statfs,
	clear_inode:    mtfs_clear_inode,
	show_options:   mtfs_show_options,
};

struct inode_operations mtfs_lustre_symlink_iops =
{
	readlink:       mtfs_readlink,
	follow_link:    mtfs_follow_link,
	put_link:       mtfs_put_link,
	permission:     mtfs_permission,
	setattr:        mtfs_setattr,
	getattr:        mtfs_getattr,
};

struct inode_operations mtfs_lustre_dir_iops =
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

struct inode_operations mtfs_lustre_main_iops =
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

struct dentry_operations mtfs_lustre_dops = {
	d_revalidate:  mtfs_d_revalidate_local,
	d_release:     mtfs_d_release,
};

struct file_operations mtfs_lustre_dir_fops =
{
	llseek:   mtfs_file_llseek,
	read:     generic_read_dir,
	readdir:  mtfs_readdir,
	ioctl:    mtfs_ioctl,
	open:     mtfs_open,
	release:  mtfs_release,
	/* TODO: fsync, do we really need open? */
};

struct file_operations mtfs_lustre_main_fops =
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

struct heal_operations mtfs_lustre_hops =
{
	ho_discard_dentry: heal_discard_dentry_sync,
};

struct mtfs_operations mtfs_lustre_operations = {
	symlink_iops:            &mtfs_lustre_symlink_iops,
	dir_iops:                &mtfs_lustre_dir_iops,
	main_iops:               &mtfs_lustre_main_iops,
	main_fops:               &mtfs_lustre_main_fops,
	dir_fops:                &mtfs_lustre_dir_fops,
	sops:                    &mtfs_lustre_sops,
	dops:                    &mtfs_lustre_dops,
	aops:                    &mtfs_aops,
	vm_ops:                  &mtfs_file_vm_ops,
	ioctl:                    NULL,
	subject_ops:              NULL,
	iupdate_ops:             &mtfs_iupdate_choose,
	io_ops:                  &mtfs_io_ops,
};

const char *lustre_supported_secondary_types[] = {
	"lustre",
	NULL,
};

struct mtfs_junction mtfs_lustre_junction = {
	mj_owner:                THIS_MODULE,
	mj_name:                 "lustre",
	mj_subject:              "SYNC_REPLICA",
	mj_primary_type:         "lustre",
	mj_secondary_types:      lustre_supported_secondary_types,
	mj_fs_ops:              &mtfs_lustre_operations,
};

static int sync_replica_lustre_init(void)
{
	int ret = 0;

	ret = junction_register(&mtfs_lustre_junction);
	if (ret) {
		MERROR("failed to register junction for lustre4, ret = %d\n", ret);
	}
	return ret;
}

static void sync_replica_lustre_exit(void)
{
	MDEBUG("unregistering mtfs sync juntion for lustre\n");

	junction_unregister(&mtfs_lustre_junction);
}

MODULE_AUTHOR("MulTi File System Workgroup");
MODULE_DESCRIPTION("mtfs's sync replica junction for lustre");
MODULE_LICENSE("GPL");

module_init(sync_replica_lustre_init);
module_exit(sync_replica_lustre_exit);
