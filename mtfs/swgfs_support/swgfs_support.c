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
#include "swgfs_support.h"

struct super_operations mtfs_swgfs_sops =
{
	alloc_inode:    mtfs_alloc_inode,
	destroy_inode:  mtfs_destroy_inode,
	drop_inode:     generic_delete_inode,
	put_super:      mtfs_put_super,
	statfs:         mtfs_statfs,
	clear_inode:    mtfs_clear_inode,
	show_options:   mtfs_show_options,
};

struct inode_operations mtfs_swgfs_symlink_iops =
{
	readlink:       mtfs_readlink,
	follow_link:    mtfs_follow_link,
	put_link:       mtfs_put_link,
	permission:     mtfs_permission,
	setattr:        mtfs_setattr,
	getattr:        mtfs_getattr,
};

struct inode_operations mtfs_swgfs_dir_iops =
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

struct inode_operations mtfs_swgfs_main_iops =
{
	permission:     mtfs_permission,
	setattr:        mtfs_setattr,
	getattr:        mtfs_getattr,
	setxattr:       mtfs_setxattr,
	getxattr:       mtfs_getxattr,
	removexattr:    mtfs_removexattr,
	listxattr:      mtfs_listxattr,
};

struct dentry_operations mtfs_swgfs_dops = {
	d_revalidate:  mtfs_d_revalidate,
	d_release:     mtfs_d_release,
};

struct file_operations mtfs_swgfs_dir_fops =
{
	llseek:   mtfs_file_llseek,
	read:     generic_read_dir,
	readdir:  mtfs_readdir,
	ioctl:    mtfs_ioctl,
	open:     mtfs_open,
	release:  mtfs_release,
};

extern struct vm_operations_struct *ll_mtfs_get_vm_ops(void);

struct page *mtfs_swgfs_nopage_branch(struct vm_area_struct *vma, unsigned long address,
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

	vm_ops = ll_mtfs_get_vm_ops();
	HASSERT(vm_ops);
	vma->vm_file = hidden_file;
	page = vm_ops->nopage(vma, address, type);
	vma->vm_file = file;
out:
	HRETURN(page);
}

struct page *mtfs_swgfs_nopage(struct vm_area_struct *vma, unsigned long address,
                         int *type)
{
	struct page *page = NULL;	
	HENTRY();

	page = mtfs_swgfs_nopage_branch(vma, address, type, mtfs_get_primary_bindex());
	if (page == NOPAGE_SIGBUS || page == NOPAGE_OOM) {
		HERROR("got addr %lu type %lx - SIGBUS\n", address, (long)type);
	}
	HRETURN(page);
}

struct vm_operations_struct mtfs_swgfs_vm_ops = {
	nopage:         mtfs_swgfs_nopage,
	populate:       filemap_populate
};

ssize_t mtfs_swgfs_file_readv(struct file *file, const struct iovec *iov,
                               unsigned long nr_segs, loff_t *ppos)
{ 
	ssize_t ret = 0;
	HENTRY();

	if (mtfs_f2info(file) != NULL) {
		ret = ll_mtfs_file_readv(file, mtfs_f2bnum(file), mtfs_f2barray(file), iov, nr_segs, ppos);
	}

	HRETURN(ret);
}

ssize_t mtfs_swgfs_file_writev(struct file *file, const struct iovec *iov,
                                unsigned long nr_segs, loff_t *ppos)
{
	ssize_t ret = 0;
	struct iovec *iov_new = NULL;
	struct iovec *iov_tmp = NULL;
	size_t length = sizeof(*iov) * nr_segs;
	HENTRY();

	HRFS_ALLOC(iov_new, length);
	if (!iov_new) {
		ret = -ENOMEM;
		goto out;
	}
	
	HRFS_ALLOC(iov_tmp, length);
	if (!iov_tmp) {
		ret = -ENOMEM;
		goto out_new_alloced;
	}	
	memcpy((char *)iov_new, (char *)iov, length); 
	
	if (mtfs_f2info(file) != NULL) {
		ret = ll_mtfs_file_writev(file, mtfs_f2bnum(file), mtfs_f2barray(file), iov, nr_segs, ppos);
	}
	mtfs_update_inode_size(file->f_dentry->d_inode);

//tmp_alloced_err:
	HRFS_FREE(iov_tmp, length);
out_new_alloced:
	HRFS_FREE(iov_new, length);
out:
	HRETURN(ret);
}

ssize_t mtfs_swgfs_file_sendfile(struct file *in_file, loff_t *ppos,
                                 size_t len, read_actor_t actor, void *target)
{
	ssize_t err = 0;
	HENTRY();

	if (mtfs_f2info(in_file) != NULL) { 
		err = ll_mtfs_file_sendfile(in_file, mtfs_f2bnum(in_file), mtfs_f2barray(in_file), ppos, len, actor, target);
	}

	HRETURN(err);
}

struct file_operations mtfs_swgfs_main_fops =
{
	llseek:     mtfs_file_llseek,
	read:       mtfs_file_read,
	aio_read:   mtfs_file_aio_read,
	write:      mtfs_file_write,
	aio_write:  mtfs_file_aio_write,
	sendfile:   mtfs_swgfs_file_sendfile, 
	readv:      mtfs_file_readv,
#if 0
	writev:     mtfs_swgfs_file_writev,
#else
	writev:     mtfs_file_writev,
#endif
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

struct address_space_operations mtfs_swgfs_aops =
{
	direct_IO:      mtfs_direct_IO,
	writepage:      mtfs_writepage,
	readpage:       mtfs_readpage,
};

#include <mtfs_stack.h>
#include <mtfs_ioctl.h>
static int mtfs_swgfs_getflags(struct inode *inode, struct file *file, unsigned long arg)
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
	ret = hidden_file->f_op->ioctl(hidden_inode, hidden_file, FSFILT_IOC_GETFLAGS, (long)&flags);
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

int mtfs_swgfs_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	HENTRY();

	switch (cmd) {
	case FSFILT_IOC_SETFLAGS:
		ret = mtfs_ioctl_write(inode, file, cmd, arg);
		break;
	case FSFILT_IOC_GETFLAGS:
		ret = mtfs_swgfs_getflags(inode, file, arg);
		break;
	default:
		ret = -ENOTTY;
		break;
	}

	HRETURN(ret);
}

struct mtfs_operations mtfs_swgfs_operations = {
	symlink_iops:            &mtfs_swgfs_symlink_iops,
	dir_iops:                &mtfs_swgfs_dir_iops,
	main_iops:               &mtfs_swgfs_main_iops,
	main_fops:               &mtfs_swgfs_main_fops,
	dir_fops:                &mtfs_swgfs_dir_fops,
	sops:                    &mtfs_swgfs_sops,
	dops:                    &mtfs_swgfs_dops,
	vm_ops:                  &mtfs_swgfs_vm_ops,
	ioctl:                   &mtfs_swgfs_ioctl,
};

const char *supported_secondary_types[] = {
	"swgfs",
	NULL,
};

struct mtfs_junction mtfs_swgfs_junction = {
	junction_owner:          THIS_MODULE,
	junction_name:           "swgfs",
	primary_type:            "swgfs",
	secondary_types:         supported_secondary_types,
	fs_ops:                  &mtfs_swgfs_operations,
};

extern int ll_mtfs_inode_set_flag(struct inode *inode, __u32 flag);
extern int ll_mtfs_inode_get_flag(struct inode *inode, __u32 *mtfs_flag);
extern int ll_mtfs_idata_init(struct inode *inode, struct inode *parent_inode, int is_primary);
extern int ll_mtfs_idata_finit(struct inode *inode);

struct lowerfs_operations lowerfs_swgfs_ops = {
	lowerfs_owner:           THIS_MODULE,
	lowerfs_type:            "swgfs",
	lowerfs_magic:           SWGFS_SUPER_MAGIC,
	lowerfs_flag:            LF_RMDIR_NO_DDLETE | LF_UNLINK_NO_DDLETE,
	lowerfs_inode_set_flag:  ll_mtfs_inode_set_flag,
	lowerfs_inode_get_flag:  ll_mtfs_inode_get_flag,
	lowerfs_idata_init:      ll_mtfs_idata_init,
	lowerfs_idata_finit:     ll_mtfs_idata_finit,
};

static int swgfs_support_init(void)
{
	int ret = 0;

	HDEBUG("registering mtfs_swgfs support\n");

	ret = lowerfs_register_ops(&lowerfs_swgfs_ops);
	if (ret) {
		HERROR("failed to register lowerfs operation: error %d\n", ret);
		goto out;
	}	

	ret = junction_register(&mtfs_swgfs_junction);
	if (ret) {
		HERROR("failed to register junction: error %d\n", ret);
		goto out_unregister_lowerfs_ops;
	}
	goto out;
out_unregister_lowerfs_ops:
	lowerfs_unregister_ops(&lowerfs_swgfs_ops);
out:
	return ret;
}

static void swgfs_support_exit(void)
{
	HDEBUG("Unregistering mtfs_swgfs support\n");
	lowerfs_unregister_ops(&lowerfs_swgfs_ops);

	junction_unregister(&mtfs_swgfs_junction);
}

MODULE_AUTHOR("Massive Storage Management Workgroup of Jiangnan Computing Technology Institute");
MODULE_DESCRIPTION("mtfs's support for ext4");
MODULE_LICENSE("GPL");

module_init(swgfs_support_init);
module_exit(swgfs_support_exit);
