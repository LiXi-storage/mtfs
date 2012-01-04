/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
 */

#include "hrfs_internal.h"

#if 0
int hrfs_ioctl_flag_convert(int hrfs_flag)
{
	int lowerfs_flag = hrfs_flag
	return lowerfs_flag;
}
EXPORT_SYMBOL(hrfs_ioctl_flag_convert);

int hrfs_inode_flag_convert(int lowerfs_iflag)
{
	int hrfs_iflag = lowerfs_iflag
	return hrfs_iflag;
}
EXPORT_SYMBOL(hrfs_inode_flag_convert);

static int hrfs_setflags(struct inode *inode, struct file *file, unsigned long arg)
{
	int ret = 0;
	struct file *hidden_file = NULL;
	struct inode *hidden_inode = NULL;
	hrfs_bindex_t bindex = 0;
	int undo_ret = 0;
	int flags = 0;
	mm_segment_t old_fs;
	int lowerfs_flag = 0;
	HENTRY();

	HASSERT(hrfs_f2info(file));

	for(bindex = 0; bindex < hrfs_f2bnum(file); bindex++) {
		hidden_file = hrfs_f2branch(file, bindex);
		hidden_inode = hrfs_i2branch(inode, bindex);
		if (hidden_file == NULL || hidden_inode == NULL) {
			HERROR("branch[%d] of file [%*s] is NULL, ioctl getflags skipped\n", 
			       bindex, file->f_dentry->d_name.len, file->f_dentry->d_name.name);
			continue;
		}

		if (hidden_file->f_op == NULL || hidden_file->f_op->ioctl == NULL) {
			HERROR("branch[%d] of file [%*s] do not support ioctl, ioctl setflags skipped\n", 
			       bindex, file->f_dentry->d_name.len, file->f_dentry->d_name.name);
			continue;
		}	
		

		hrfs_ioctl_flag_convert();
		ret = hidden_file->f_op->ioctl(hidden_inode, hidden_file, HRFS_IOC_SETFLAGS, arg);
		if (ret < 0) {
			HERROR("ioctl setflags branch[%d] of file [%*s] failed, ret = %d\n",
			       bindex, file->f_dentry->d_name.len, file->f_dentry->d_name.name, ret);
			bindex--;
			goto out_undo;
		}
	}

	if (get_user(flags, (int *)arg)) {
		ret = -EFAULT;
		goto out;
	}
	inode->i_flags = hrfs_lowerfs_to_inode_flags(flags | HRFS_RESERVED_FL);
	goto out;
out_undo:
	for(; bindex >= 0; bindex--) {
		flags = inode_to_ext_flags(inode->i_flags, 1);
		hidden_file = hrfs_f2branch(file, bindex);
		hidden_inode = hrfs_i2branch(inode, bindex);
		if (!hidden_file) {
			continue;
		}
		old_fs = get_fs();
		set_fs(get_ds());
		undo_ret = hidden_file->f_op->ioctl(hidden_inode, hidden_file, FS_IOC_SETFLAGS, (long)&flags);
		set_fs(old_fs);
		if (undo_ret < 0) {
			HERROR("undo ioctl setflags branch[%d] of file [%*s] failed, ret = %d\n",
			       bindex, file->f_dentry->d_name.len, file->f_dentry->d_name.name, undo_ret);
		}
	}
out:
	HRETURN(ret);
}
#endif

static int hrfs_user_get_state(struct inode *inode, struct hrfs_user_flag __user *user_state, hrfs_bindex_t max_bnum)
{
	hrfs_bindex_t bnum = hrfs_i2bnum(inode);
	hrfs_bindex_t bindex = 0;
	__u32 branch_flag = 0;
	int ret = 0;
	struct hrfs_user_flag *state = NULL;
	int state_size = 0;
	struct inode *hidden_inode = NULL;
	struct lowerfs_operations *lowerfs_ops = NULL;

	HASSERT(bnum <= max_bnum);

	state_size = hrfs_user_flag_size(bnum);
	HRFS_ALLOC(state, state_size);
	if (state == NULL) {
		ret = -ENOMEM;
		goto out;
	}
	state->state_size = state_size;
	state->bnum = bnum;

	for(bindex = 0; bindex < bnum; bindex++) {
		hidden_inode = hrfs_i2branch(inode, bindex);
		lowerfs_ops = hrfs_i2bops(inode, bindex);
		HASSERT(hidden_inode);
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
	return ret;	
}

static int hrfs_user_set_state(struct inode *inode, struct hrfs_user_flag __user *user_state)
{
	hrfs_bindex_t bnum = hrfs_i2bnum(inode);
	hrfs_bindex_t bindex = 0;
	int ret = 0;
	struct hrfs_user_flag *state = NULL;
	int state_size = 0;
	struct inode *hidden_inode = NULL;
	struct lowerfs_operations *lowerfs_ops = NULL;
	
	state_size = hrfs_user_flag_size(bnum);
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
		hidden_inode = hrfs_i2branch(inode, bindex);
		lowerfs_ops = hrfs_i2bops(inode, bindex);
		HASSERT(hidden_inode);
		ret = lowerfs_inode_set_flag(lowerfs_ops, hidden_inode, state->state[bindex].flag);
		if (ret) {
			goto recover;
		}
	}
	goto free_state;

recover:
	/* TODO: If fail, we should recover */
	LBUG();
free_state:	
	HRFS_FREE(state, state_size);
out:
	return ret;
}

int hrfs_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	int err = 0;
	int val = 0;
	struct hrfs_operations *operations = NULL;
	HENTRY();

	switch (cmd) {
	case HRFS_IOCTL_GET_FLAG:
		err = hrfs_user_get_state(inode, (struct hrfs_user_flag __user *)arg, HRFS_BRANCH_MAX);
		break;
	case HRFS_IOCTL_SET_FLAG:
		HERROR("HRFS_IOCTL_SET_FLAG in\n");
		err = hrfs_user_set_state(inode, (struct hrfs_user_flag __user *)arg);
		break;
	case HRFS_IOCTL_GET_DEBUG_LEVEL:
		//val = fist_get_debug_value();
		printk("IOCTL GET: send arg %d\n", val);
		err = put_user(val, (int *)arg);
		break;

	case HRFS_IOCTL_SET_DEBUG_LEVEL:
		err = get_user(val, (int *)arg);
		if (err)
			break;
		HDEBUG("IOCTL SET: got arg %d\n", val);
		if (val < 0 || val > 20) {
			err = -EINVAL;
			break;
		}
		break;
	case HRFS_IOCTL_RULE_ADD:
		err = -EINVAL;
		break;
	case HRFS_IOCTL_RULE_DEL:
		err = -EINVAL;
		break;
	case HRFS_IOCTL_RULE_LIST:
		err = -EINVAL;
		break;
#ifdef LIXI_20110518
			{
				struct lov_user_md *lump = NULL;
				int lumlen = 0;

				lumlen = swgfs_getstripe(hidden_file->f_dentry, &lump, lumlen);
				if (lumlen <= 0) {
					HERROR("getstripe failed: %d\n", err);
				}
				HDEBUG("stripe_count: %d stripe_size: %u "
					"stripe_offset: %d stripe_pattern: %d "
					"\n",
					lump->lmm_stripe_count == (__u16)-1 ? -1 :
						lump->lmm_stripe_count,
					lump->lmm_stripe_size,
					lump->lmm_stripe_offset == (__u16)-1 ? -1 :
						lump->lmm_stripe_offset,
					lump->lmm_pattern - 1);
				swgfs_getstripe_finished(lump, lumlen);
			}
#endif /* LIXI_20110518 */
	default:
			operations = hrfs_i2ops(inode);
			if (operations->ioctl == NULL) {
				err = -ENOTTY;
			} else {
				err = operations->ioctl(inode, file, cmd, arg);
			}
	} /* end of outer switch statement */

	HRETURN(err);
}
EXPORT_SYMBOL(hrfs_ioctl);
