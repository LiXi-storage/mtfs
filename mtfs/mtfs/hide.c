/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include "mtfs_internal.h"

#define MTFS_HIDDEN_SUPER_MAGIC 0xbadc0ffee

static void mtfs_hideen_put_super(struct super_block *sb)
{
	HENTRY();

	_HRETURN();
}

static struct super_operations mtfs_hidden_sops =
{
	statfs:         simple_statfs,
	put_super:      mtfs_hideen_put_super,
};

static int mtfs_hideen_read_super(struct super_block *sb, void *input, int silent)
{
	int ret = 0;
	struct inode *root = NULL;
	HENTRY();

	sb->s_blocksize = 4096;
	sb->s_blocksize_bits = 12;
	sb->s_magic = MTFS_HIDDEN_SUPER_MAGIC;
	sb->s_maxbytes = 0; //PAGE_CACHE_MAXBYTES;
	sb->s_flags |= MS_RDONLY;
	sb->s_op = &mtfs_hidden_sops;

	root = new_inode(sb);
	if (root == NULL) {
		HERROR("Can't make root inode\n");
		ret = -ENOMEM;
		goto out;
	}

	/* returns -EIO for every operation */
	/* make_bad_inode(root); -- badness - can't umount */
	/* apparently we need to be a directory for the mount to finish */
	root->i_mode = S_IFDIR;

	sb->s_root = d_alloc_root(root);
	if (sb->s_root == NULL) {
		HERROR("Can't make root dentry\n");
		iput(root);
		ret = -ENOMEM;
		goto out;
	}
out:
	HRETURN(ret);
}

static int mtfs_hideen_get_sb(struct file_system_type *fs_type,
                              int flags, const char *dev_name,
                              void *raw_data, struct vfsmount *mnt)
{
	return get_sb_nodev(fs_type, flags, raw_data, mtfs_hideen_read_super, mnt);
}

struct file_system_type mtfs_hidden_fs_type = {
	owner:     THIS_MODULE,
	name:      MTFS_HIDDEN_FS_TYPE,
	get_sb:    mtfs_hideen_get_sb,
	kill_sb:   generic_shutdown_super,
};

