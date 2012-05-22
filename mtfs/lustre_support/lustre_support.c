/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <mtfs_support.h>
#include <mtfs_super.h>
#include <mtfs_dentry.h>
#include <mtfs_inode.h>
#include <mtfs_mmap.h>
#include <mtfs_flag.h>
#include <mtfs_junction.h>
#include "lustre_support.h"

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
	lookup:	        mtfs_lookup,
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

struct dentry_operations mtfs_lustre_dops = {
	d_revalidate:  mtfs_d_revalidate,
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
};

extern struct vm_operations_struct *mtfs_ll_get_vm_ops(void);

struct page *mtfs_lustre_nopage_branch(struct vm_area_struct *vma, unsigned long address,
                         int *type, mtfs_bindex_t bindex)
{
	struct page *page = NULL;
	struct file *file = NULL;
	struct file *hidden_file = NULL;
	struct vm_operations_struct *vm_ops = NULL;
	int ret = 0;
	HENTRY();

	file = vma->vm_file;
	HASSERT(file);
	hidden_file = mtfs_f_choose_branch(file, HRFS_DATA_VALID);
	if (IS_ERR(hidden_file)) {
		ret = PTR_ERR(hidden_file);
		HERROR("choose branch failed, ret = %d\n", ret);
		goto out;
	}

	vm_ops = mtfs_ll_get_vm_ops();
	HASSERT(vm_ops);
	vma->vm_file = hidden_file;
	page = vm_ops->nopage(vma, address, type);
	vma->vm_file = file;
out:
	HRETURN(page);
}

struct page *mtfs_lustre_nopage(struct vm_area_struct *vma, unsigned long address,
                         int *type)
{
	struct page *page = NULL;	
	HENTRY();

	page = mtfs_lustre_nopage_branch(vma, address, type, mtfs_get_primary_bindex());
	if (page == NOPAGE_SIGBUS || page == NOPAGE_OOM) {
		HERROR("got addr %lu type %lx - SIGBUS\n", address, (long)type);
	}
	HRETURN(page);
}

struct vm_operations_struct mtfs_lustre_vm_ops = {
	nopage:         mtfs_lustre_nopage,
	populate:       filemap_populate
};

ssize_t mtfs_lustre_file_writev(struct file *file, const struct iovec *iov,
                                unsigned long nr_segs, loff_t *ppos)
{
	ssize_t size = 0;
	int ret = 0;
	mtfs_operation_result_t result;
	struct mtfs_operation_list *oplist = NULL;
	struct inode *inode = NULL;
	HENTRY();

	inode = file->f_dentry->d_inode;
        oplist = mtfs_oplist_build_keep_order(inode);
	if (unlikely(oplist == NULL)) {
		HERROR("failed to build operation list\n");
		ret = -ENOMEM;
		goto out;
	}

	if (mtfs_i_wlock_down_interruptible(inode)) {
		HERROR("Interupted by signal\n");
		/* Never return an error like -EINTR */
		goto out_free_oplist;
	}
	ret = mtfs_ll_file_writev(file, iov, nr_segs, ppos, oplist);
	if (ret) {
		HERROR("failed to writev, ret = %d\n", ret);
	}
	mtfs_i_wlock_up(inode);

	mtfs_oplist_check(oplist);
	if (oplist->success_bnum <= 0) {
		result = mtfs_oplist_result(oplist);
		size = result.size;
		goto out_free_oplist;
	}

	ret = mtfs_oplist_update(file->f_dentry->d_inode, oplist);
	if (ret) {
		HERROR("failed to update inode\n");
		HBUG();
	}

	HASSERT(oplist->success_bnum > 0);
	result = mtfs_oplist_result(oplist);
	size = result.size;
	*ppos = *ppos + size;

out_free_oplist:
	mtfs_oplist_free(oplist);
out:
        if (ret) {
                size = ret;
        }
	HRETURN(size);
}

struct file_operations mtfs_lustre_main_fops =
{
	llseek:     mtfs_file_llseek,
	read:       mtfs_file_read,
	aio_read:   mtfs_file_aio_read,
	write:      mtfs_file_write,
	aio_write:  mtfs_file_aio_write,
//	sendfile:   mtfs_lustre_file_sendfile, 
	readv:      mtfs_file_readv,
        writev:     mtfs_lustre_file_writev,
	readdir:    mtfs_readdir,
	poll:       mtfs_poll,
	ioctl:      mtfs_ioctl,
	mmap:       mtfs_file_mmap,
	open:       mtfs_open,
	release:    mtfs_release,
	fsync:      mtfs_fsync,
	fasync:     mtfs_fasync,
	lock:       mtfs_lock,
	flock:      mtfs_flock,
};

struct address_space_operations mtfs_lustre_aops =
{
	direct_IO:      mtfs_direct_IO,
	writepage:      mtfs_writepage,
	readpage:       mtfs_readpage,
};

#include <mtfs_stack.h>
#include <mtfs_ioctl.h>
static int mtfs_lustre_getflags(struct inode *inode, struct file *file, unsigned long arg)
{
	int ret = 0;
	mtfs_bindex_t bindex = -1;
	struct file *hidden_file = NULL;
	struct inode *hidden_inode = NULL;
	int flags = 0;
	mm_segment_t old_fs;
	HENTRY();

	ret = mtfs_i_choose_bindex(inode, HRFS_ATTR_VALID, &bindex);
	if (ret) {
		HERROR("choose bindex failed, ret = %d\n", ret);
		goto out;
	}

	HASSERT(bindex >=0 && bindex < mtfs_i2bnum(inode));
	hidden_file = mtfs_f2branch(file, bindex);
	hidden_inode = mtfs_i2branch(inode, bindex);
	HASSERT(hidden_file);
	HASSERT(hidden_inode);


	old_fs = get_fs();
	set_fs(get_ds());
	HASSERT(hidden_file);
	HASSERT(hidden_file->f_op);
#ifdef HAVE_UNLOCKED_IOCTL
	HASSERT(hidden_file->f_op->unlocked_ioctl);
	ret = hidden_file->f_op->unlocked_ioctl(hidden_file, FSFILT_IOC_GETFLAGS, (long)&flags);
#else
	HASSERT(hidden_file->f_op->ioctl);
	ret = hidden_file->f_op->ioctl(hidden_inode, hidden_file, FSFILT_IOC_GETFLAGS, (long)&flags);
#endif

	set_fs(old_fs);
	if (ret) {
		HERROR("ioctl getflags branch[%d] of file [%*s] failed, ret = %d\n",
		       bindex, file->f_dentry->d_name.len, file->f_dentry->d_name.name, ret);
		goto out;
	}
	put_user(flags, (int __user *)arg);

	fsstack_copy_attr_all(inode, hidden_inode, mtfs_get_nlinks);
out:
	HRETURN(ret);
}

int mtfs_lustre_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	HENTRY();

	switch (cmd) {
	case FSFILT_IOC_SETFLAGS:
		ret = mtfs_ioctl_write(inode, file, cmd, arg);
		break;
	case FSFILT_IOC_GETFLAGS:
		ret = mtfs_lustre_getflags(inode, file, arg);
		break;
	default:
		ret = -ENOTTY;
		break;
	}

	HRETURN(ret);
}

struct mtfs_operations mtfs_lustre_operations = {
	symlink_iops:            &mtfs_lustre_symlink_iops,
	dir_iops:                &mtfs_lustre_dir_iops,
	main_iops:               &mtfs_lustre_main_iops,
	main_fops:               &mtfs_lustre_main_fops,
	dir_fops:                &mtfs_lustre_dir_fops,
	sops:                    &mtfs_lustre_sops,
	dops:                    &mtfs_lustre_dops,
	vm_ops:                  &mtfs_lustre_vm_ops,
	ioctl:                   &mtfs_lustre_ioctl,
};

const char *supported_secondary_types[] = {
	"lustre",
	NULL,
};

struct mtfs_junction mtfs_lustre_junction = {
	junction_owner:          THIS_MODULE,
	junction_name:           "lustre",
	primary_type:            "lustre",
	secondary_types:         supported_secondary_types,
	fs_ops:                  &mtfs_lustre_operations,
};

struct lowerfs_operations lowerfs_lustre_ops = {
	lowerfs_owner:           THIS_MODULE,
	lowerfs_type:            "lustre",
	lowerfs_magic:           LUSTRE_SUPER_MAGIC,
	lowerfs_flag:            LF_RMDIR_NO_DDLETE | LF_UNLINK_NO_DDLETE,
	//lowerfs_inode_set_flag:  lowerfs_inode_set_flag_default,
	//lowerfs_inode_get_flag:  lowerfs_inode_get_flag_default,
	lowerfs_inode_set_flag:  mtfs_ll_inode_set_flag,
	lowerfs_inode_get_flag:  mtfs_ll_inode_get_flag,
	lowerfs_idata_init:      NULL,
	lowerfs_idata_finit:     NULL,
};

static int lustre_support_init(void)
{
	int ret = 0;

	HDEBUG("registering mtfs_lustre support\n");

	ret = lowerfs_register_ops(&lowerfs_lustre_ops);
	if (ret) {
		HERROR("failed to register lowerfs operation: error %d\n", ret);
		goto out;
	}	

	ret = junction_register(&mtfs_lustre_junction);
	if (ret) {
		HERROR("failed to register junction: error %d\n", ret);
		goto out_unregister_lowerfs_ops;
	}
	goto out;
out_unregister_lowerfs_ops:
	lowerfs_unregister_ops(&lowerfs_lustre_ops);
out:
	return ret;
}

static void lustre_support_exit(void)
{
	HDEBUG("Unregistering mtfs_lustre support\n");
	lowerfs_unregister_ops(&lowerfs_lustre_ops);

	junction_unregister(&mtfs_lustre_junction);
}

MODULE_AUTHOR("Massive Storage Management Workgroup of Jiangnan Computing Technology Institute");
MODULE_DESCRIPTION("mtfs's support for ext4");
MODULE_LICENSE("GPL");

module_init(lustre_support_init);
module_exit(lustre_support_exit);
