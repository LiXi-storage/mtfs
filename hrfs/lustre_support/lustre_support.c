/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
 */

#include <hrfs_support.h>
#include <hrfs_super.h>
#include <hrfs_dentry.h>
#include <hrfs_inode.h>
#include <hrfs_mmap.h>
#include <hrfs_flag.h>
#include <hrfs_junction.h>
#include "lustre_support.h"

struct super_operations hrfs_lustre_sops =
{
	alloc_inode:    hrfs_alloc_inode,
	destroy_inode:  hrfs_destroy_inode,
	drop_inode:     generic_delete_inode,
	put_super:      hrfs_put_super,
	statfs:         hrfs_statfs,
	clear_inode:    hrfs_clear_inode,
	show_options:   hrfs_show_options,
};

struct inode_operations hrfs_lustre_symlink_iops =
{
	readlink:       hrfs_readlink,
	follow_link:    hrfs_follow_link,
	put_link:       hrfs_put_link,
	permission:     hrfs_permission,
	setattr:        hrfs_setattr,
	getattr:        hrfs_getattr,
};

struct inode_operations hrfs_lustre_dir_iops =
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

struct inode_operations hrfs_lustre_main_iops =
{
	permission:     hrfs_permission,
	setattr:        hrfs_setattr,
	getattr:        hrfs_getattr,
	setxattr:       hrfs_setxattr,
	getxattr:       hrfs_getxattr,
	removexattr:    hrfs_removexattr,
	listxattr:      hrfs_listxattr,
};

struct dentry_operations hrfs_lustre_dops = {
	d_revalidate:  hrfs_d_revalidate,
	d_release:     hrfs_d_release,
};

struct file_operations hrfs_lustre_dir_fops =
{
	llseek:   hrfs_file_llseek,
	read:     generic_read_dir,
	readdir:  hrfs_readdir,
	ioctl:    hrfs_ioctl,
	open:     hrfs_open,
	release:  hrfs_release,
};

extern struct vm_operations_struct *hrfs_ll_get_vm_ops(void);

struct page *hrfs_lustre_nopage_branch(struct vm_area_struct *vma, unsigned long address,
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

	vm_ops = hrfs_ll_get_vm_ops();
	HASSERT(vm_ops);
	vma->vm_file = hidden_file;
	page = vm_ops->nopage(vma, address, type);
	vma->vm_file = file;
out:
	HRETURN(page);
}

struct page *hrfs_lustre_nopage(struct vm_area_struct *vma, unsigned long address,
                         int *type)
{
	struct page *page = NULL;	
	HENTRY();

	page = hrfs_lustre_nopage_branch(vma, address, type, hrfs_get_primary_bindex());
	if (page == NOPAGE_SIGBUS || page == NOPAGE_OOM) {
		HERROR("got addr %lu type %lx - SIGBUS\n", address, (long)type);
	}
	HRETURN(page);
}

struct vm_operations_struct hrfs_lustre_vm_ops = {
	nopage:         hrfs_lustre_nopage,
	populate:       filemap_populate
};

ssize_t hrfs_lustre_file_writev(struct file *file, const struct iovec *iov,
                         unsigned long nr_segs, loff_t *ppos)
{
	ssize_t size = 0;
	int ret = 0;
	hrfs_operation_result_t result;
	struct hrfs_operation_list *oplist = NULL;
	HENTRY();

        oplist = hrfs_oplist_build_keep_order(file->f_dentry->d_inode);
	if (unlikely(oplist == NULL)) {
		HERROR("failed to build operation list\n");
		ret = -ENOMEM;
		goto out;
	}

	hrfs_ll_file_writev(file, iov, nr_segs, ppos, oplist);

	hrfs_oplist_check(oplist);
	if (oplist->success_bnum <= 0) {
		result = hrfs_oplist_result(oplist);
		size = result.size;
		goto out_free_oplist;
	}

	ret = hrfs_oplist_update(file->f_dentry->d_inode, oplist);
	if (ret) {
		HERROR("failed to update inode\n");
		HBUG();
	}

	HASSERT(oplist->success_bnum > 0);
	result = hrfs_oplist_result(oplist);
	size = result.size;
	*ppos = *ppos + size;

out_free_oplist:
	hrfs_oplist_free(oplist);
out:
        if (ret) {
                size = ret;
        }
	HRETURN(size);
}

struct file_operations hrfs_lustre_main_fops =
{
	llseek:     hrfs_file_llseek,
	read:       hrfs_file_read,
	aio_read:   hrfs_file_aio_read,
	write:      hrfs_file_write,
	aio_write:  hrfs_file_aio_write,
//	sendfile:   hrfs_lustre_file_sendfile, 
	readv:      hrfs_file_readv,
        writev:     hrfs_lustre_file_writev,
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

struct address_space_operations hrfs_lustre_aops =
{
	direct_IO:      hrfs_direct_IO,
	writepage:      hrfs_writepage,
	readpage:       hrfs_readpage,
};

#include <hrfs_stack.h>
#include <hrfs_ioctl.h>
static int hrfs_lustre_getflags(struct inode *inode, struct file *file, unsigned long arg)
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

	fsstack_copy_attr_all(inode, hidden_inode, hrfs_get_nlinks);
out:
	HRETURN(ret);
}

int hrfs_lustre_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	HENTRY();

	switch (cmd) {
	case FSFILT_IOC_SETFLAGS:
		ret = hrfs_ioctl_write(inode, file, cmd, arg);
		break;
	case FSFILT_IOC_GETFLAGS:
		ret = hrfs_lustre_getflags(inode, file, arg);
		break;
	default:
		ret = -ENOTTY;
		break;
	}

	HRETURN(ret);
}

struct hrfs_operations hrfs_lustre_operations = {
	symlink_iops:            &hrfs_lustre_symlink_iops,
	dir_iops:                &hrfs_lustre_dir_iops,
	main_iops:               &hrfs_lustre_main_iops,
	main_fops:               &hrfs_lustre_main_fops,
	dir_fops:                &hrfs_lustre_dir_fops,
	sops:                    &hrfs_lustre_sops,
	dops:                    &hrfs_lustre_dops,
	vm_ops:                  &hrfs_lustre_vm_ops,
	ioctl:                   &hrfs_lustre_ioctl,
};

const char *supported_secondary_types[] = {
	"lustre",
	NULL,
};

struct hrfs_junction hrfs_lustre_junction = {
	junction_owner:          THIS_MODULE,
	junction_name:           "lustre",
	primary_type:            "lustre",
	secondary_types:         supported_secondary_types,
	fs_ops:                  &hrfs_lustre_operations,
};

struct lowerfs_operations lowerfs_lustre_ops = {
	lowerfs_owner:           THIS_MODULE,
	lowerfs_type:            "lustre",
	lowerfs_magic:           LUSTRE_SUPER_MAGIC,
	lowerfs_flag:            LF_RMDIR_NO_DDLETE | LF_UNLINK_NO_DDLETE,
	//lowerfs_inode_set_flag:  lowerfs_inode_set_flag_default,
	//lowerfs_inode_get_flag:  lowerfs_inode_get_flag_default,
	lowerfs_inode_set_flag:  hrfs_ll_inode_set_flag,
	lowerfs_inode_get_flag:  hrfs_ll_inode_get_flag,
	lowerfs_idata_init:      NULL,
	lowerfs_idata_finit:     NULL,
};

static int lustre_support_init(void)
{
	int ret = 0;

	HDEBUG("registering hrfs_lustre support\n");

	ret = lowerfs_register_ops(&lowerfs_lustre_ops);
	if (ret) {
		HERROR("failed to register lowerfs operation: error %d\n", ret);
		goto out;
	}	

	ret = junction_register(&hrfs_lustre_junction);
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
	HDEBUG("Unregistering hrfs_lustre support\n");
	lowerfs_unregister_ops(&lowerfs_lustre_ops);

	junction_unregister(&hrfs_lustre_junction);
}

MODULE_AUTHOR("Massive Storage Management Workgroup of Jiangnan Computing Technology Institute");
MODULE_DESCRIPTION("hrfs's support for ext4");
MODULE_LICENSE("GPL");

module_init(lustre_support_init);
module_exit(lustre_support_exit);
