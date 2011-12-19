/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
 */

#include "hrfs_internal.h"

struct inode *hrfs_alloc_inode(struct super_block *sb)
{
	struct hrfs_inode_info *inode_info = NULL;
	struct inode *inode = NULL;
	HENTRY();

	inode_info = hrfs_ii_alloc();
	if (unlikely(!inode_info)) {
		goto out;
	}

	/* Init inode */
	inode_init_once(&inode_info->vfs_inode);

	inode = &inode_info->vfs_inode;

out:
	HRETURN(inode);
}
EXPORT_SYMBOL(hrfs_alloc_inode);

void hrfs_destroy_inode(struct inode *inode)
{
	struct hrfs_inode_info *inode_info = hrfs_i2info(inode);
	HENTRY();

	/*
	 * TODO: alloc branch array in hrfs_alloc_inode?
	 * Since branch array not alloced in hrfs_alloc_inode, 
	 * it may be null after get_new_inode()
	 * called hrfs_struct inodeest and failed to alloc memory
	 */
	if (inode_info->barray != NULL) {
		hrfs_ii_branch_free(inode_info);
	}
	hrfs_ii_free(inode_info);
	_HRETURN();
}
EXPORT_SYMBOL(hrfs_destroy_inode);

void hrfs_put_super(struct super_block *sb)
{
	hrfs_bindex_t bindex = 0;
	HENTRY();

	if (hrfs_s2info(sb)) {
		for (bindex = 0; bindex < hrfs_s2bnum(sb); bindex++) {
			mntput(hrfs_s2mntbranch(sb, bindex));
			dput(hrfs_s2brecover(sb, bindex));
		}
		HASSERT(hrfs_s2dev(sb));
		hrfs_freedev(hrfs_s2dev(sb));
		hrfs_s_free(sb);
	}

	_HRETURN();
}
EXPORT_SYMBOL(hrfs_put_super);

int hrfs_statfs(struct dentry *dentry, struct kstatfs *buf)
{
	int err = 0;
	struct super_block *hidden_sb = NULL;
	struct kstatfs rsb;
	hrfs_bindex_t global_bindex = 0;
	hrfs_bindex_t bindex1 = 0;
	hrfs_bindex_t bnum = 0;
	dev_t *hidden_s_dev = NULL;
	int matched = 0;
	struct super_block *sb = dentry->d_sb;
	HENTRY();

	HRFS_ALLOC(hidden_s_dev, sizeof(*hidden_s_dev) * hrfs_s2bnum(sb));
	if (!hidden_s_dev) {
		err = -ENOMEM;
		goto out;
	}

	/* HRFS_ALLOC will do this */
	// memset(buf, 0, sizeof(struct kstatfs));

	buf->f_type = HRFS_SUPER_MAGIC;

	bnum = hrfs_s2bnum(sb);
	for (global_bindex = 0; global_bindex < bnum; global_bindex++) {

		hidden_sb = hrfs_s2branch(sb, global_bindex);

		/* store the current s_dev for checking any
		 * duplicate additions for super block data
		 */
		hidden_s_dev[global_bindex] = hidden_sb->s_dev;
		matched = 0;
		for (bindex1 = 0; bindex1 < global_bindex; bindex1++) {
			/* check the hidden super blocks with any previously 
			 * added super block data.  If match, continue
			 */
			if (hidden_s_dev[bindex1] == hidden_sb->s_dev) {
				matched = 1;
				break;
			}
		}
		if (matched) {
			continue;
		}

		err = vfs_statfs(hrfs_d2branch(sb->s_root, global_bindex), &rsb);

		buf->f_bsize = rsb.f_bsize;
		buf->f_blocks += rsb.f_blocks;
		buf->f_bfree += rsb.f_bfree;
		buf->f_bavail += rsb.f_bavail;
		buf->f_files += rsb.f_files;
		buf->f_ffree += rsb.f_ffree;
	}

	HRFS_FREE(hidden_s_dev, sizeof(*hidden_s_dev) * hrfs_s2bnum(sb));

out:
	HRETURN(err);
}
EXPORT_SYMBOL(hrfs_statfs);

/*
 * Called by iput() when the inode reference count reaches zero
 * and the inode is not hashed anywhere.  Used to clear anything
 * that needs to be, before the inode is completely destroyed and put
 * on the inode free list.
 */
void hrfs_clear_inode(struct inode *inode)
{
	hrfs_bindex_t bindex = 0;
	hrfs_bindex_t bnum = 0;
	struct inode *hidden_inode = NULL;
	struct lowerfs_operations *lowerfs_ops = NULL;
	HENTRY();

	for (bindex = 0; bindex < hrfs_i2bnum(inode); bindex++) {
		if (hrfs_i2branch(inode, bindex) == NULL) {
			continue;
		}
		lowerfs_ops = hrfs_i2bops(inode, bindex);
		lowerfs_idata_finit(lowerfs_ops, hrfs_i2branch(inode, bindex));
	}
	/*
	 * Decrease a reference to a hidden_inode, which was increased
	 * by our read_inode when inode was inited.
	 */
	bnum = hrfs_i2bnum(inode);
	for (bindex = 0; bindex < bnum; bindex++) {
		hidden_inode = hrfs_i2branch(inode, bindex);
		if (hidden_inode == NULL)
			continue;
		iput(hidden_inode);
	}

	_HRETURN();
}
EXPORT_SYMBOL(hrfs_clear_inode);

int hrfs_show_options(struct seq_file *m, struct vfsmount *mnt)
{
	HENTRY();

	seq_printf(m, ",only_for_test");

	HRETURN(0);
}
EXPORT_SYMBOL(hrfs_show_options);

static struct dentry *hrfs_get_parent(struct dentry *dentry) {
	return d_find_alias(dentry->d_inode);
}

struct export_operations hrfs_export_ops = {
	get_parent:     hrfs_get_parent,
};

struct super_operations hrfs_sops =
{
	alloc_inode:    hrfs_alloc_inode,
	destroy_inode:  hrfs_destroy_inode,
	drop_inode:     generic_delete_inode,
	put_super:      hrfs_put_super,
	statfs:         hrfs_statfs,
	clear_inode:    hrfs_clear_inode,
	show_options:   hrfs_show_options,
};



