/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <linux/module.h>
#include <mtfs_selfheal.h>
#include <debug.h>
#include <memory.h>
#include "heal_internal.h"
#include "dentry_internal.h"
#include "ioctl_internal.h"
#include "support_internal.h"

static int mtfs_user_get_state(struct inode *inode, struct file *file, struct mtfs_user_flag __user *user_state, mtfs_bindex_t max_bnum)
{
	mtfs_bindex_t bnum = mtfs_i2bnum(inode);
	mtfs_bindex_t bindex = 0;
	__u32 branch_flag = 0;
	int ret = 0;
	struct mtfs_user_flag *state = NULL;
	int state_size = 0;
	struct inode *hidden_inode = NULL;
	struct lowerfs_operations *lowerfs_ops = NULL;
	MENTRY();

	MASSERT(bnum <= max_bnum);

	state_size = mtfs_user_flag_size(bnum);
	MTFS_ALLOC(state, state_size);
	if (state == NULL) {
		ret = -ENOMEM;
		goto out;
	}
	state->state_size = state_size;
	state->bnum = bnum;

	for(bindex = 0; bindex < bnum; bindex++) {
		hidden_inode = mtfs_i2branch(inode, bindex);
		lowerfs_ops = mtfs_i2bops(inode, bindex);
		if (hidden_inode == NULL) {
			(state->state[bindex]).flag = 0xffff;
			continue;
		}
		ret = lowerfs_inode_get_flag(lowerfs_ops, hidden_inode, &branch_flag);
		if (ret) {
			goto free_state;
		}
		(state->state[bindex]).flag = branch_flag;
	}

	ret = copy_to_user(user_state, state, state_size);

free_state:	
	MTFS_FREE(state, state_size);
out:
	MRETURN(ret);	
}

static int mtfs_user_set_state(struct inode *inode, struct file *file, struct mtfs_user_flag __user *user_state)
{
	mtfs_bindex_t bnum = mtfs_i2bnum(inode);
	mtfs_bindex_t bindex = 0;
	int ret = 0;
	struct mtfs_user_flag *state = NULL;
	int state_size = 0;
	struct inode *hidden_inode = NULL;
	struct lowerfs_operations *lowerfs_ops = NULL;
	MENTRY();

	state_size = mtfs_user_flag_size(bnum);
	MTFS_ALLOC(state, state_size);
	if (state == NULL) {
		ret = -ENOMEM;
		goto out;
	}
	
	ret = copy_from_user(state, user_state, state_size);
	if (ret) {
		ret = -EFAULT;
		goto free_state;
	}
	
	if (state->bnum != bnum) {
		MERROR("bnum (%d) is not valid, expect %d\n", state->bnum, bnum);
		ret = -EINVAL;
		goto free_state;
	}
	
	if (state->state_size != state_size) {
		MERROR("state_size (%d) is not valid, expect %d\n", state->state_size, state_size);
		ret = -EINVAL;
		goto free_state;
	}

	for (bindex = 0; bindex < bnum; bindex++) {
		hidden_inode = mtfs_i2branch(inode, bindex);
		if (hidden_inode != NULL) {
			if (unlikely(!mtfs_flag_is_valid(state->state[bindex].flag))) {
				ret = -EPERM;
				goto out;
			}
		}
	}

	for (bindex = 0; bindex < bnum; bindex++) {
		hidden_inode = mtfs_i2branch(inode, bindex);
		lowerfs_ops = mtfs_i2bops(inode, bindex);
		if (hidden_inode == NULL) {
			MDEBUG("branch[%d] of inode is NULL, skipping\n", bindex);
		} else {
			ret = lowerfs_inode_set_flag(lowerfs_ops, hidden_inode, state->state[bindex].flag);
			if (ret) {
				MERROR("lowerfs_inode_set_flag failed, ret = %d\n", ret);
				goto recover;
			}
		}
	}
	goto free_state;

recover:
	/* TODO: If fail, we should recover */
	MBUG();
free_state:	
	MTFS_FREE(state, state_size);
out:
	MRETURN(ret);
}

static int mtfs_remove_branch(struct dentry *d_parent, const char *name, mtfs_bindex_t bindex)
{
	int ret                        = 0;
	struct dentry *hidden_d_child  = NULL;
	struct dentry *hidden_d_parent = mtfs_d2branch(d_parent, bindex);
	struct dentry *d_child         = NULL;
	MENTRY();

	if (hidden_d_parent|| hidden_d_parent->d_inode) {
		hidden_d_child = mtfs_dchild_remove(hidden_d_parent, name);
		if (IS_ERR(hidden_d_child)) {
			ret = PTR_ERR(hidden_d_child);
			goto out;
		}

#ifdef LIXI_20120717
		mutex_lock(&d_parent->d_inode->i_mutex);
		{
			d_child = lookup_one_len(hidden_d_child->d_name.name,
			                         d_parent, hidden_d_child->d_name.len);
			dput(hidden_d_child);

			hidden_d_child = mtfs_lookup_branch(d_child, bindex);
			if (IS_ERR(hidden_d_child)) {
				mtfs_d2branch(d_child, bindex) = NULL;
				ret = PTR_ERR(hidden_d_child);
			} else {
				mtfs_d2branch(d_child, bindex) = hidden_d_child;
			}
			mtfs_inode_set(d_child->d_inode, d_child);
			dput(d_child);
		}
		mutex_unlock(&d_parent->d_inode->i_mutex);
#else
		dput(hidden_d_child);
		mutex_lock(&d_parent->d_inode->i_mutex);
		{
			d_child = lookup_one_len(hidden_d_child->d_name.name,
				                 d_parent, hidden_d_child->d_name.len);
			lock_dentry(d_child);
			d_child->d_flags |= DCACHE_MTFS_INVALID;
			unlock_dentry(d_child);
			dput(d_child);
		}
		mutex_unlock(&d_parent->d_inode->i_mutex);
#endif
	} else {
		if (hidden_d_parent == NULL) {
			MDEBUG("branch[%d] of dentry [%.*s] is NULL\n", bindex,
			       d_parent->d_name.len, d_parent->d_parent->d_name.name);
		} else {
			MDEBUG("branch[%d] of dentry [%.*s] is negative\n", bindex,
			       d_parent->d_name.len, d_parent->d_parent->d_name.name);
		}
		ret = -ENOENT;
	}

out:
	MRETURN(ret);
}

static int mtfs_user_remove_branch(struct inode *parent_inode, struct file *parent_file, struct mtfs_remove_branch_info __user *user_remove_info)
{
	int ret = 0;
	struct mtfs_remove_branch_info *remove_info = NULL;
	MENTRY();

	MTFS_ALLOC_PTR(remove_info);
	if (remove_info == NULL) {
		goto out;
	}

	ret = copy_from_user(remove_info, user_remove_info, sizeof(struct mtfs_remove_branch_info));
	if (ret) {
		ret = -EFAULT;
		goto out_free_info;
	}

	if (remove_info->bindex < 0 || remove_info->bindex >= mtfs_i2bnum(parent_inode)) {
		ret = -EFAULT;
		goto out_free_info;
	}

	ret = mtfs_remove_branch(parent_file->f_dentry, remove_info->name, remove_info->bindex);
out_free_info:
	MTFS_FREE_PTR(remove_info);
out:
	MRETURN(ret);
}

static long __vfs_ioctl(struct file *filp, unsigned int cmd,
		        unsigned long arg, int is_kernel_ds)
{
	int ret = -ENOTTY;
	mm_segment_t old_fs = {0};

	if (!filp->f_op) {
		goto out;
	}

	if (is_kernel_ds) {
		old_fs = get_fs();
		set_fs(get_ds());
	}

	if (filp->f_op->unlocked_ioctl) {
		ret = filp->f_op->unlocked_ioctl(filp, cmd, arg);
		if (ret == -ENOIOCTLCMD) {
			ret = -EINVAL;
		}
		goto out_ds;
	} else if (filp->f_op->ioctl) {
		//lock_kernel();
#ifdef HAVE_PATH_IN_STRUCT_FILE
		ret = filp->f_op->ioctl(filp->f_path.dentry->d_inode,
					filp, cmd, arg);
#else
		ret = filp->f_op->ioctl(filp->f_dentry->d_inode,
					filp, cmd, arg);
#endif
		//unlock_kernel();
	}
out_ds:
	if (is_kernel_ds) {
		set_fs(old_fs);
	}
 out:
	return ret;
}


static int mtfs_ioctl_do_branch(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg, mtfs_bindex_t bindex, int is_kernel_ds)
{
	int ret = 0;
	struct file *hidden_file = mtfs_f2branch(file, bindex);
	struct inode *hidden_inode = mtfs_i2branch(inode, bindex);
	MENTRY();

	ret = mtfs_device_branch_errno(mtfs_i2dev(inode), bindex, BOPS_MASK_WRITE);
	if (ret) {
		MDEBUG("branch[%d] is abandoned\n", bindex);
		goto out; 
	}

	if (hidden_file && hidden_inode) {
		ret = __vfs_ioctl(hidden_file, cmd, arg, is_kernel_ds);
	} else {
		MERROR("branch[%d] of file [%.*s] is NULL, ioctl setflags skipped\n", 
		       bindex, file->f_dentry->d_name.len, file->f_dentry->d_name.name);
		ret = -ENOENT;
	}

out:
	MRETURN(ret);
}

int mtfs_ioctl_write(struct inode *inode, struct file *file,
                     unsigned int cmd, unsigned long arg, int is_kernel_ds)
{
	int ret = 0;
	mtfs_bindex_t i = 0;
	mtfs_bindex_t bindex = 0;
	struct mtfs_operation_list *list = NULL;
	mtfs_operation_result_t result = {0};	
	MENTRY();

	MASSERT(mtfs_f2info(file));

	list = mtfs_oplist_build(inode);
	if (unlikely(list == NULL)) {
		MERROR("failed to build operation list\n");
		ret = -ENOMEM;
		goto out;
	}

	if (list->latest_bnum == 0) {
		MERROR("file [%.*s] has no valid branch, please check it\n",
		       file->f_dentry->d_name.len, file->f_dentry->d_name.name);
		if (!(mtfs_i2dev(inode)->no_abort)) {
			ret = -EIO;
			goto out_free_oplist;
		}
	}

	for (i = 0; i < mtfs_f2bnum(file); i++) {
		bindex = list->op_binfo[i].bindex;
		ret = mtfs_ioctl_do_branch(inode, file, cmd, arg, bindex, is_kernel_ds);
		result.ret = ret;
		mtfs_oplist_setbranch(list, i, (ret == 0 ? 1 : 0), result);
		if (i == list->latest_bnum - 1) {
			mtfs_oplist_check(list);
			if (list->success_latest_bnum <= 0) {
				MDEBUG("operation failed for all latest %d branches\n", list->latest_bnum);
				if (!(mtfs_i2dev(inode)->no_abort)) {
					result = mtfs_oplist_result(list);
					ret = result.ret;
					goto out_free_oplist;
				}
			}
		}
	}

	mtfs_oplist_check(list);
	if (list->success_bnum <= 0) {
		result = mtfs_oplist_result(list);
		ret = result.ret;
		goto out_free_oplist;
	}

	ret = mtfs_oplist_update(inode, list);
	if (ret) {
		MERROR("failed to update inode\n");
		MBUG();
	}

	mtfs_update_inode_attr(inode);
out_free_oplist:
	mtfs_oplist_free(list);
out:
	MRETURN(ret);
}
EXPORT_SYMBOL(mtfs_ioctl_write);

int mtfs_ioctl_read(struct inode *inode, struct file *file,
                    unsigned int cmd, unsigned long arg, int is_kernel_ds)
{
	mtfs_bindex_t bindex = -1;
	int ret = 0;
	MENTRY();

	ret = mtfs_i_choose_bindex(inode, MTFS_BRANCH_VALID, &bindex);
	if (ret) {
		MERROR("choose bindex failed, ret = %d\n", ret);
		goto out;
	}

	MASSERT(bindex >=0 && bindex < mtfs_i2bnum(inode));
	ret = mtfs_ioctl_do_branch(inode, file, cmd, arg, bindex, is_kernel_ds);
out:
	MRETURN(ret);
}
EXPORT_SYMBOL(mtfs_ioctl_read);

int mtfs_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct mtfs_operations *operations = NULL;
	MENTRY();

	switch (cmd) {
	case MTFS_IOCTL_GET_FLAG:
		ret = mtfs_user_get_state(inode, file,
		                          (struct mtfs_user_flag __user *)arg, MTFS_BRANCH_MAX);
		break;
	case MTFS_IOCTL_SET_FLAG:
		ret = mtfs_user_set_state(inode,  file,
		                          (struct mtfs_user_flag __user *)arg);
		break;
	case MTFS_IOCTL_REMOVE_BRANCH:
		ret = mtfs_user_remove_branch(inode, file,
		                              (struct mtfs_remove_branch_info __user *)arg);
		break;
	case MTFS_IOCTL_RULE_ADD:
		ret = -EINVAL;
		break;
	case MTFS_IOCTL_RULE_DEL:
		ret = -EINVAL;
		break;
	case MTFS_IOCTL_RULE_LIST:
		ret = -EINVAL;
		break;
	default:
			operations = mtfs_i2ops(inode);
			if (operations->ioctl == NULL) {
				ret = -ENOTTY;
			} else {
				ret = operations->ioctl(inode, file, cmd, arg);
			}
	} /* end of outer switch statement */

	MRETURN(ret);
}
EXPORT_SYMBOL(mtfs_ioctl);
