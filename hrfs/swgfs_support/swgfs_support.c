/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <hrfs_support.h>
#include <hrfs_super.h>
#include <hrfs_dentry.h>
#include <hrfs_inode.h>
#include <hrfs_mmap.h>
#include <hrfs_flag.h>
#include <hrfs_junction.h>
#include "swgfs_support.h"

struct super_operations hrfs_swgfs_sops =
{
	alloc_inode:    hrfs_alloc_inode,
	destroy_inode:  hrfs_destroy_inode,
	drop_inode:     generic_delete_inode,
	put_super:      hrfs_put_super,
	statfs:         hrfs_statfs,
	clear_inode:    hrfs_clear_inode,
	show_options:   hrfs_show_options,
};

struct inode_operations hrfs_swgfs_symlink_iops =
{
	readlink:       hrfs_readlink,
	follow_link:    hrfs_follow_link,
	put_link:       hrfs_put_link,
	permission:     hrfs_permission,
	setattr:        hrfs_setattr,
	getattr:        hrfs_getattr,
};

struct inode_operations hrfs_swgfs_dir_iops =
{
	create:	        hrfs_create,
	lookup:	        hrfs_lookup,
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

struct inode_operations hrfs_swgfs_main_iops =
{
	permission:     hrfs_permission,
	setattr:        hrfs_setattr,
	getattr:        hrfs_getattr,
	setxattr:       hrfs_setxattr,
	getxattr:       hrfs_getxattr,
	removexattr:    hrfs_removexattr,
	listxattr:      hrfs_listxattr,
};

struct dentry_operations hrfs_swgfs_dops = {
	d_revalidate:  hrfs_d_revalidate,
	d_release:     hrfs_d_release,
};

struct file_operations hrfs_swgfs_dir_fops =
{
	llseek:   hrfs_file_llseek,
	read:     generic_read_dir,
	readdir:  hrfs_readdir,
	ioctl:    hrfs_ioctl,
	open:     hrfs_open,
	release:  hrfs_release,
};

extern struct vm_operations_struct *ll_hrfs_get_vm_ops(void);

struct page *hrfs_swgfs_nopage_branch(struct vm_area_struct *vma, unsigned long address,
                         int *type, hrfs_bindex_t bindex)
{
	struct page *page = NULL;
	struct file *file = NULL;
	struct file *hidden_file = NULL;
	struct vm_operations_struct *vm_ops = NULL;
	int ret = 0;
	HENTRY();

	file = vma->vm_file;
	HASSERT(file);
	hidden_file = hrfs_f_choose_branch(file, HRFS_DATA_VALID);
	if (IS_ERR(hidden_file)) {
		ret = PTR_ERR(hidden_file);
		HERROR("choose branch failed, ret = %d\n", ret);
		goto out;
	}

	vm_ops = ll_hrfs_get_vm_ops();
	HASSERT(vm_ops);
	vma->vm_file = hidden_file;
	page = vm_ops->nopage(vma, address, type);
	vma->vm_file = file;
out:
	HRETURN(page);
}

struct page *hrfs_swgfs_nopage(struct vm_area_struct *vma, unsigned long address,
                         int *type)
{
	struct page *page = NULL;	
	HENTRY();

	page = hrfs_swgfs_nopage_branch(vma, address, type, hrfs_get_primary_bindex());
	if (page == NOPAGE_SIGBUS || page == NOPAGE_OOM) {
		HERROR("got addr %lu type %lx - SIGBUS\n", address, (long)type);
	}
	HRETURN(page);
}

struct vm_operations_struct hrfs_swgfs_vm_ops = {
	nopage:         hrfs_swgfs_nopage,
	populate:       filemap_populate
};

ssize_t hrfs_swgfs_file_readv(struct file *file, const struct iovec *iov,
                               unsigned long nr_segs, loff_t *ppos)
{ 
	ssize_t ret = 0;
	HENTRY();

	if (hrfs_f2info(file) != NULL) {
		ret = ll_hrfs_file_readv(file, hrfs_f2bnum(file), hrfs_f2barray(file), iov, nr_segs, ppos);
	}

	HRETURN(ret);
}

ssize_t hrfs_swgfs_file_writev(struct file *file, const struct iovec *iov,
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
	
	if (hrfs_f2info(file) != NULL) {
		ret = ll_hrfs_file_writev(file, hrfs_f2bnum(file), hrfs_f2barray(file), iov, nr_segs, ppos);
	}
	hrfs_update_inode_size(file->f_dentry->d_inode);

//tmp_alloced_err:
	HRFS_FREE(iov_tmp, length);
out_new_alloced:
	HRFS_FREE(iov_new, length);
out:
	HRETURN(ret);
}

ssize_t hrfs_swgfs_file_sendfile(struct file *in_file, loff_t *ppos,
                                 size_t len, read_actor_t actor, void *target)
{
	ssize_t err = 0;
	HENTRY();

	if (hrfs_f2info(in_file) != NULL) { 
		err = ll_hrfs_file_sendfile(in_file, hrfs_f2bnum(in_file), hrfs_f2barray(in_file), ppos, len, actor, target);
	}

	HRETURN(err);
}

struct file_operations hrfs_swgfs_main_fops =
{
	llseek:     hrfs_file_llseek,
	read:       hrfs_file_read,
	aio_read:   hrfs_file_aio_read,
	write:      hrfs_file_write,
	aio_write:  hrfs_file_aio_write,
	sendfile:   hrfs_swgfs_file_sendfile, 
	readv:      hrfs_file_readv,
#if 0
	writev:     hrfs_swgfs_file_writev,
#else
	writev:     hrfs_file_writev,
#endif
	readdir:    hrfs_readdir,
	poll:       hrfs_poll,
	ioctl:      hrfs_ioctl,
	mmap:       hrfs_file_mmap,
	open:       hrfs_open,
	release:    hrfs_release,
	fsync:      hrfs_fsync,
	fasync:     hrfs_fasync,
	lock:       hrfs_lock,
	flock:      hrfs_flock,
};

struct address_space_operations hrfs_swgfs_aops =
{
	direct_IO:      hrfs_direct_IO,
	writepage:      hrfs_writepage,
	readpage:       hrfs_readpage,
};

#include <hrfs_stack.h>
#include <hrfs_ioctl.h>
static int hrfs_swgfs_getflags(struct inode *inode, struct file *file, unsigned long arg)
{
	int ret = 0;
	hrfs_bindex_t bindex = -1;
	struct file *hidden_file = NULL;
	struct inode *hidden_inode = NULL;
	int flags = 0;
	mm_segment_t old_fs;
	HENTRY();

	ret = hrfs_i_choose_bindex(inode, HRFS_ATTR_VALID, &bindex);
	if (ret) {
		HERROR("choose bindex failed, ret = %d\n", ret);
		goto out;
	}
	HASSERT(bindex >=0 && bindex < hrfs_i2bnum(inode));
	hidden_file = hrfs_f2branch(file, bindex);
	hidden_inode = hrfs_i2branch(inode, bindex);
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

	fsstack_copy_attr_all(inode, hidden_inode, hrfs_get_nlinks);
out:
	HRETURN(ret);
}

int hrfs_swgfs_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	HENTRY();

	switch (cmd) {
	case FSFILT_IOC_SETFLAGS:
		ret = hrfs_ioctl_write(inode, file, cmd, arg);
		break;
	case FSFILT_IOC_GETFLAGS:
		ret = hrfs_swgfs_getflags(inode, file, arg);
		break;
	default:
		ret = -ENOTTY;
		break;
	}

	HRETURN(ret);
}

struct hrfs_operations hrfs_swgfs_operations = {
	symlink_iops:            &hrfs_swgfs_symlink_iops,
	dir_iops:                &hrfs_swgfs_dir_iops,
	main_iops:               &hrfs_swgfs_main_iops,
	main_fops:               &hrfs_swgfs_main_fops,
	dir_fops:                &hrfs_swgfs_dir_fops,
	sops:                    &hrfs_swgfs_sops,
	dops:                    &hrfs_swgfs_dops,
	vm_ops:                  &hrfs_swgfs_vm_ops,
	ioctl:                   &hrfs_swgfs_ioctl,
};

const char *supported_secondary_types[] = {
	"swgfs",
	NULL,
};

struct hrfs_junction hrfs_swgfs_junction = {
	junction_owner:          THIS_MODULE,
	junction_name:           "swgfs",
	primary_type:            "swgfs",
	secondary_types:         supported_secondary_types,
	fs_ops:                  &hrfs_swgfs_operations,
};

extern int ll_hrfs_inode_set_flag(struct inode *inode, __u32 flag);
extern int ll_hrfs_inode_get_flag(struct inode *inode, __u32 *hrfs_flag);
extern int ll_hrfs_idata_init(struct inode *inode, struct inode *parent_inode, int is_primary);
extern int ll_hrfs_idata_finit(struct inode *inode);

struct lowerfs_operations lowerfs_swgfs_ops = {
	lowerfs_owner:           THIS_MODULE,
	lowerfs_type:            "swgfs",
	lowerfs_magic:           SWGFS_SUPER_MAGIC,
	lowerfs_flag:            LF_RMDIR_NO_DDLETE | LF_UNLINK_NO_DDLETE,
	lowerfs_inode_set_flag:  ll_hrfs_inode_set_flag,
	lowerfs_inode_get_flag:  ll_hrfs_inode_get_flag,
	lowerfs_idata_init:      ll_hrfs_idata_init,
	lowerfs_idata_finit:     ll_hrfs_idata_finit,
};

static int swgfs_support_init(void)
{
	int ret = 0;

	HDEBUG("registering hrfs_swgfs support\n");

	ret = lowerfs_register_ops(&lowerfs_swgfs_ops);
	if (ret) {
		HERROR("failed to register lowerfs operation: error %d\n", ret);
		goto out;
	}	

	ret = junction_register(&hrfs_swgfs_junction);
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
	HDEBUG("Unregistering hrfs_swgfs support\n");
	lowerfs_unregister_ops(&lowerfs_swgfs_ops);

	junction_unregister(&hrfs_swgfs_junction);
}

MODULE_AUTHOR("Massive Storage Management Workgroup of Jiangnan Computing Technology Institute");
MODULE_DESCRIPTION("hrfs's support for ext4");
MODULE_LICENSE("GPL");

module_init(swgfs_support_init);
module_exit(swgfs_support_exit);
