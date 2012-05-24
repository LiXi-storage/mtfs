/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include "mtfs_internal.h"

#define qstr_pair(qstr)        (qstr)->len, (qstr)->name
#define d_name_pair(dentry)    qstr_pair(&(dentry)->d_name)

int mtfs_dentry_dump(struct dentry *dentry)
{
	int ret = 0;
	struct inode *inode = NULL;

	if (dentry && !IS_ERR(dentry)) {
		HDEBUG("dentry: error = %ld\n", PTR_ERR(dentry));
		ret = -1;
		goto out;
	}

	/* do not call dget_parent() here */
	HDEBUG("dentry: name = %.*s?/%.*s, fstype = %s,"
		"d_count = %d, d_flags = 0x%x\n",
		d_name_pair(dentry->d_parent), d_name_pair(dentry), dentry->d_sb ? s_type(dentry->d_sb) : "??",
		atomic_read(&dentry->d_count), dentry->d_flags);

	inode = dentry->d_inode;
	if (inode && !IS_ERR(inode)) {
		ret = mtfs_inode_dump(inode);
	}
out:
	return ret;
}

static int mtfs_d_revalidate_branch(struct dentry *dentry, struct nameidata *nd, mtfs_bindex_t bindex)
{
	int ret = 0;
	struct dentry *hidden_dentry = mtfs_d2branch(dentry, bindex);
	HENTRY();

	hidden_dentry = mtfs_d2branch(dentry, bindex);
	if (likely(hidden_dentry)) {
		//HASSERT(!d_unhashed(hidden_dentry));
		ret = hidden_dentry->d_op->d_revalidate(hidden_dentry, nd);
		if (ret <= 0) {
			HDEBUG("branch[%d] of dentry [%*s] is invalid, ret = %d\n",
			       bindex, dentry->d_name.len, dentry->d_name.name, ret);
		}
	} else {
		HDEBUG("branch[%d] of dentry [%*s] is NULL\n",
		       bindex, dentry->d_name.len, dentry->d_name.name);
		ret = -ENOENT;
	}

	HRETURN(ret);	
}

int mtfs_d_revalidate(struct dentry *dentry, struct nameidata *nd)
{
	int ret = 0;
	mtfs_bindex_t bindex = 0;
	mtfs_bindex_t bnum = 0;
	HENTRY();

	HDEBUG("d_revalidate [%*s]\n", dentry->d_name.len, dentry->d_name.name);
	HASSERT(mtfs_d2info(dentry));

	/* 
	 * The revalidation must occur for all the common files across branches
	 */
	bnum = mtfs_d2bnum(dentry);

	for (bindex = 0; bindex < bnum; bindex++) {
		if (mtfs_d2branch(dentry, bindex) == NULL) {
			continue;
		}

		if (nd) {
			struct nameidata new_nd = {0};

			/* Revalidate the attr, see BUG351 */
			new_nd.flags |= LOOKUP_REVAL;
			ret = mtfs_d_revalidate_branch(dentry, &new_nd, bindex);
		} else {
			ret = mtfs_d_revalidate_branch(dentry, NULL, bindex);
		}
		if (ret <= 0) {
			goto out_d_drop;
		}
	}

	/* Primary barnch is valid for sure */
	ret = 1;

	/* Do we need to update attr? */
	if (dentry->d_inode) {
		struct inode *hidden_inode = mtfs_i_choose_branch(dentry->d_inode, HRFS_ATTR_VALID);
		if (IS_ERR(hidden_inode)) {
			ret = PTR_ERR(hidden_inode);
			HERROR("choose branch failed, ret = %d\n", ret);
			goto out;
		}
		fsstack_copy_attr_all(dentry->d_inode, hidden_inode, mtfs_get_nlinks);
	}

	/*
	 * Lustre says: Never execute intents for mount points.
	 * Attributes will be fixed up in ll_inode_revalidate_it.
	 * Our mtfs_getattr will call lustre's, and call ll_inode_revalidate_it to fix attributes.
	 */
	if (d_mountpoint(dentry)) {
		goto out;
	}

	if (nd && !(nd->flags & (LOOKUP_CONTINUE|LOOKUP_PARENT))) {
		if (dentry->d_inode == NULL) {
			if (nd->flags & LOOKUP_CREATE) {
				HDEBUG("revalidate have to return negative dentry\n");
			} else {
				/* try to get inode */
				HDEBUG("do not know how to get a inode\n");
			}
		} else {
			if ((nd->flags & LOOKUP_OPEN)) { /*Open*/
				if (S_ISREG(dentry->d_inode->i_mode) ||
					S_ISDIR(dentry->d_inode->i_mode)) {
					struct file *filp;
					HDEBUG("name = %*s, flags = 0x%x\n", dentry->d_name.len, dentry->d_name.name, nd->intent.open.flags);
					filp = lookup_instantiate_filp(nd, dentry, NULL);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,17))
					/* 2.6.1[456] have a bug in open_namei() that forgets to check
					 * nd->intent.open.file for error, so we need to return it as lookup's result
					 * instead
					 */
					if (IS_ERR(filp)) {
						dentry = (struct dentry*) filp;
					}
#endif
				} else {
					HDEBUG("do not handle open intent for a file other than dir or file\n");
				}
			}
		}
	}
	goto out;
out_d_drop:
	HASSERT(ret <= 0);
	if (ret == 0 && !d_mountpoint(dentry)) {
		/* Do not d_drop though dentry is invalid, since it is mountpoint. */
		d_drop(dentry);
	}
out:
	HRETURN(ret);
}
EXPORT_SYMBOL(mtfs_d_revalidate);

void mtfs_d_release(struct dentry *dentry)
{
	struct dentry *hidden_dentry = NULL;
	mtfs_bindex_t bindex = 0;
	mtfs_bindex_t bnum = 0;
	HENTRY();

	HDEBUG("d_release [%*s]\n", dentry->d_name.len, dentry->d_name.name);
	HASSERT(d_unhashed(dentry));

	/* This could be a negative dentry */
	if (unlikely(!mtfs_d2info(dentry))) {
		HDEBUG("dentry [%*s] has no private data\n",
		       dentry->d_name.len, dentry->d_name.name);
		goto out;
	}
	
	/* Release all hidden dentries */
	bnum = mtfs_d2bnum(dentry);
	for (bindex = 0; bindex < bnum; bindex++) {
		hidden_dentry = mtfs_d2branch(dentry, bindex);
		if (hidden_dentry) {
			dput(hidden_dentry);
		}
	}
	mtfs_d_free(dentry);
out:
	_HRETURN();
}
EXPORT_SYMBOL(mtfs_d_release);

struct dentry_operations mtfs_dops = {
	d_revalidate:  mtfs_d_revalidate,
	d_release:     mtfs_d_release,
};
