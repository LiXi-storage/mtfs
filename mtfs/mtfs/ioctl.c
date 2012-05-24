/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include "mtfs_internal.h"

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
	HENTRY();

	HASSERT(bnum <= max_bnum);

	state_size = mtfs_user_flag_size(bnum);
	HRFS_ALLOC(state, state_size);
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
	HRFS_FREE(state, state_size);
out:
	HRETURN(ret);	
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
	HENTRY();

	state_size = mtfs_user_flag_size(bnum);
	HRFS_ALLOC(state, state_size);
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
		HERROR("bnum (%d) is not valid, expect %d\n", state->bnum, bnum);
		ret = -EINVAL;
		goto free_state;
	}
	
	if (state->state_size != state_size) {
		HERROR("state_size (%d) is not valid, expect %d\n", state->state_size, state_size);
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
			HDEBUG("branch[%d] of inode is NULL, skipping\n", bindex);
		} else {
			ret = lowerfs_inode_set_flag(lowerfs_ops, hidden_inode, state->state[bindex].flag);
			if (ret) {
				HERROR("lowerfs_inode_set_flag failed, ret = %d\n", ret);
				goto recover;
			}
		}
	}
	goto free_state;

recover:
	/* TODO: If fail, we should recover */
	HBUG();
free_state:	
	HRFS_FREE(state, state_size);
out:
	HRETURN(ret);
}

static int mtfs_remove_branch(struct dentry *d_parent, const char *name, mtfs_bindex_t bindex)
{
	int ret = 0;
	struct dentry *d_child = NULL;
	struct dentry *hidden_d_parent = mtfs_d2branch(d_parent, bindex);
	HENTRY();

	if (hidden_d_parent|| hidden_d_parent->d_inode) {
		d_child = mtfs_dchild_remove(hidden_d_parent, name);
		if (IS_ERR(d_child)) {
			ret = PTR_ERR(d_child);
			goto out;
		} else {
			dput(d_child);
		}
	} else {
		if (hidden_d_parent == NULL) {
			HDEBUG("branch[%d] of dentry [%*s] is NULL\n", bindex,
			       d_parent->d_name.len, d_parent->d_parent->d_name.name);
		} else {
			HDEBUG("branch[%d] of dentry [%*s] is negative\n", bindex,
			       d_parent->d_name.len, d_parent->d_parent->d_name.name);
		}
		ret = -ENOENT;
	}
out:
	HRETURN(ret);
}

static int mtfs_user_remove_branch(struct inode *parent_inode, struct file *parent_file, struct mtfs_remove_branch_info __user *user_remove_info)
{
	int ret = 0;
	struct mtfs_remove_branch_info *remove_info = NULL;
	HENTRY();

	HRFS_ALLOC_PTR(remove_info);
	if (remove_info == NULL) {
		goto out;
	}

	ret = copy_from_user(remove_info, user_remove_info, sizeof(struct mtfs_remove_branch_info));
	if (ret) {
		ret = -EFAULT;
		goto out_free_info;
	}

	ret = mtfs_remove_branch(parent_file->f_dentry, remove_info->name, remove_info->bindex);
out_free_info:
	HRFS_FREE_PTR(remove_info);
out:
	HRETURN(ret);
}

static int mtfs_ioctl_do_branch(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg, mtfs_bindex_t bindex)
{
	int ret = 0;
	struct file *hidden_file = mtfs_f2branch(file, bindex);
	struct inode *hidden_inode = mtfs_i2branch(inode, bindex);
	HENTRY();

	ret = mtfs_device_branch_errno(mtfs_i2dev(inode), bindex, BOPS_MASK_WRITE);
	if (ret) {
		HDEBUG("branch[%d] is abandoned\n", bindex);
		goto out; 
	}

	if (hidden_file && hidden_inode) {
		HASSERT(hidden_file->f_op);
#ifdef HAVE_UNLOCKED_IOCTL
		HASSERT(hidden_file->f_op->unlocked_ioctl);
		ret = hidden_file->f_op->unlocked_ioctl(hidden_file, cmd, arg);
#else
		HASSERT(hidden_file->f_op->ioctl);
		ret = hidden_file->f_op->ioctl(hidden_inode, hidden_file, cmd, arg);
#endif
	} else {
		HERROR("branch[%d] of file [%*s] is NULL, ioctl setflags skipped\n", 
		       bindex, file->f_dentry->d_name.len, file->f_dentry->d_name.name);
		ret = -ENOENT;
	}

out:
	HRETURN(ret);
}

int mtfs_ioctl_write(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	mtfs_bindex_t i = 0;
	mtfs_bindex_t bindex = 0;
	struct mtfs_operation_list *list = NULL;
	mtfs_operation_result_t result = {0};	
	HENTRY();

	HASSERT(mtfs_f2info(file));

	list = mtfs_oplist_build(inode);
	if (unlikely(list == NULL)) {
		HERROR("failed to build operation list\n");
		ret = -ENOMEM;
		goto out;
	}

	if (list->latest_bnum == 0) {
		HERROR("file [%*s] has no valid branch, please check it\n",
		       file->f_dentry->d_name.len, file->f_dentry->d_name.name);
		if (!(mtfs_i2dev(inode)->no_abort)) {
			ret = -EIO;
			goto out_free_oplist;
		}
	}

	for (i = 0; i < mtfs_f2bnum(file); i++) {
		bindex = list->op_binfo[i].bindex;
		ret = mtfs_ioctl_do_branch(inode, file, cmd, arg, bindex);
		result.ret = ret;
		mtfs_oplist_setbranch(list, i, (ret == 0 ? 1 : 0), result);
		if (i == list->latest_bnum - 1) {
			mtfs_oplist_check(list);
			if (list->success_latest_bnum <= 0) {
				HDEBUG("operation failed for all latest %d branches\n", list->latest_bnum);
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
		HERROR("failed to update inode\n");
		HBUG();
	}

	mtfs_update_inode_attr(inode);
out_free_oplist:
	mtfs_oplist_free(list);
out:
	HRETURN(ret);
}
EXPORT_SYMBOL(mtfs_ioctl_write);

int mtfs_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct mtfs_operations *operations = NULL;
	HENTRY();

	switch (cmd) {
	case HRFS_IOCTL_GET_FLAG:
		ret = mtfs_user_get_state(inode, file, (struct mtfs_user_flag __user *)arg, HRFS_BRANCH_MAX);
		break;
	case HRFS_IOCTL_SET_FLAG:
		ret = mtfs_user_set_state(inode,  file, (struct mtfs_user_flag __user *)arg);
		break;
	case HRFS_IOCTL_REMOVE_BRANCH:
		ret = mtfs_user_remove_branch(inode, file, (struct mtfs_remove_branch_info __user *)arg);
		break;
	case HRFS_IOCTL_RULE_ADD:
		ret = -EINVAL;
		break;
	case HRFS_IOCTL_RULE_DEL:
		ret = -EINVAL;
		break;
	case HRFS_IOCTL_RULE_LIST:
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

	HRETURN(ret);
}
EXPORT_SYMBOL(mtfs_ioctl);
