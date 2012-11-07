/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <linux/mount.h>
#include <linux/statfs.h>
#include <linux/seq_file.h>
#include <linux/fs.h>
#include <linux/module.h>
#ifdef HAVE_LINUX_EXPORTFS_H
#include <linux/exportfs.h>
#endif
#include <debug.h>
#include <memory.h>
#include <mtfs_super.h>
#include <mtfs_dentry.h>
#include "device_internal.h"
#include "inode_internal.h"
#include "lowerfs_internal.h"
#include "main_internal.h"
#include "subject_internal.h"
#include "async_internal.h"

struct inode *mtfs_alloc_inode(struct super_block *sb)
{
	struct mtfs_inode_info *inode_info = NULL;
	struct inode *inode = NULL;
	MENTRY();

	inode_info = mtfs_ii_alloc();
	if (unlikely(!inode_info)) {
		goto out;
	}

	/* Init inode */
	inode_init_once(&inode_info->mii_inode);

	inode = &inode_info->mii_inode;

out:
	MRETURN(inode);
}
EXPORT_SYMBOL(mtfs_alloc_inode);

void mtfs_destroy_inode(struct inode *inode)
{
	struct mtfs_inode_info *inode_info = mtfs_i2info(inode);
	MENTRY();

	masync_bucket_cleanup(mtfs_i2bucket(inode));
	mtfs_ii_free(inode_info);
	_MRETURN();
}
EXPORT_SYMBOL(mtfs_destroy_inode);

void mtfs_put_super(struct super_block *sb)
{
	mtfs_bindex_t bindex = 0;
	MENTRY();

	if (mtfs_s2info(sb)) {
		mtfs_subject_fini(sb);
		MASSERT(mtfs_s2dev(sb));
		mtfs_freedev(mtfs_s2dev(sb));
		mtfs_reserve_fini(sb);
		for (bindex = 0; bindex < mtfs_s2bnum(sb); bindex++) {
			mntput(mtfs_s2mntbranch(sb, bindex));
		}
		mtfs_s_free(sb);
	}

	_MRETURN();
}
EXPORT_SYMBOL(mtfs_put_super);

int mtfs_statfs(struct dentry *dentry, struct kstatfs *buf)
{
	int ret = 0;
	struct super_block *hidden_sb = NULL;
	struct kstatfs rsb;
	mtfs_bindex_t bindex = 0;
	mtfs_bindex_t i = 0;
	mtfs_bindex_t bnum = 0;
	dev_t *hidden_s_dev = NULL;
	int matched = 0;
	struct super_block *sb = dentry->d_sb;
#ifdef HAVE_VFS_STATFS_PATH
	struct path hidden_path;
#endif /* HAVE_VFS_STATFS_PATH */
	MENTRY();

	MTFS_ALLOC(hidden_s_dev, sizeof(*hidden_s_dev) * mtfs_s2bnum(sb));
	if (!hidden_s_dev) {
		ret = -ENOMEM;
		goto out;
	}

	/* MTFS_ALLOC will do this */
	// memset(buf, 0, sizeof(struct kstatfs));

	buf->f_type = MTFS_SUPER_MAGIC;

	bnum = mtfs_s2bnum(sb);
	for (bindex = 0; bindex < bnum; bindex++) {
		hidden_sb = mtfs_s2branch(sb, bindex);

		/* store the current s_dev for checking any
		 * duplicate additions for super block data
		 */
		hidden_s_dev[bindex] = hidden_sb->s_dev;
		matched = 0;
		for (i = 0; i < bindex; i++) {
			/*
			 * check the hidden super blocks with any previously 
			 * added super block data.  If match, continue
			 */
			if (hidden_s_dev[i] == hidden_sb->s_dev) {
				matched = 1;
				break;
			}
		}
		if (matched) {
			continue;
		}

#ifdef HAVE_VFS_STATFS_PATH
		hidden_path.dentry = mtfs_d2branch(sb->s_root, bindex);
		hidden_path.mnt = mtfs_s2mntbranch(sb, bindex);
		ret = vfs_statfs(&hidden_path, &rsb);
#else /* !HAVE_VFS_STATFS_PATH */
		ret = vfs_statfs(mtfs_d2branch(sb->s_root, bindex), &rsb);
#endif /* !HAVE_VFS_STATFS_PATH */
		buf->f_bsize = rsb.f_bsize;
		buf->f_blocks += rsb.f_blocks;
		buf->f_bfree += rsb.f_bfree;
		buf->f_bavail += rsb.f_bavail;
		buf->f_files += rsb.f_files;
		buf->f_ffree += rsb.f_ffree;
	}

	MTFS_FREE(hidden_s_dev, sizeof(*hidden_s_dev) * mtfs_s2bnum(sb));

out:
	MRETURN(ret);
}
EXPORT_SYMBOL(mtfs_statfs);

/*
 * Called by iput() when the inode reference count reaches zero
 * and the inode is not hashed anywhere.  Used to clear anything
 * that needs to be, before the inode is completely destroyed and put
 * on the inode free list.
 */
void mtfs_clear_inode(struct inode *inode)
{
	mtfs_bindex_t bindex = 0;
	mtfs_bindex_t bnum = 0;
	struct inode *hidden_inode = NULL;
	struct lowerfs_operations *lowerfs_ops = NULL;
	MENTRY();

	for (bindex = 0; bindex < mtfs_i2bnum(inode); bindex++) {
		if (mtfs_i2branch(inode, bindex) == NULL) {
			continue;
		}
		lowerfs_ops = mtfs_i2bops(inode, bindex);
		lowerfs_idata_finit(lowerfs_ops, mtfs_i2branch(inode, bindex));
	}
	/*
	 * Decrease a reference to a hidden_inode, which was increased
	 * by our read_inode when inode was inited.
	 */
	bnum = mtfs_i2bnum(inode);
	for (bindex = 0; bindex < bnum; bindex++) {
		hidden_inode = mtfs_i2branch(inode, bindex);
		if (hidden_inode == NULL)
			continue;
		iput(hidden_inode);
	}

	_MRETURN();
}
EXPORT_SYMBOL(mtfs_clear_inode);

int mtfs_show_options(struct seq_file *m, struct vfsmount *mnt)
{
	MENTRY();

	seq_printf(m, ",only_for_test");

	MRETURN(0);
}
EXPORT_SYMBOL(mtfs_show_options);

static struct dentry *mtfs_get_parent(struct dentry *child) {
	return d_find_alias(child->d_inode);
}

struct export_operations mtfs_export_ops = {
	get_parent:     mtfs_get_parent,
};

struct super_operations mtfs_sops =
{
	alloc_inode:    mtfs_alloc_inode,
	destroy_inode:  mtfs_destroy_inode,
	drop_inode:     generic_delete_inode,
	put_super:      mtfs_put_super,
	statfs:         mtfs_statfs,
	clear_inode:    mtfs_clear_inode,
	show_options:   mtfs_show_options,
};
