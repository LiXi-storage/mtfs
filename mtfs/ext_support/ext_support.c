/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <linux/module.h>
#include <mtfs_heal.h>
#include <mtfs_support.h>
#include <mtfs_super.h>
#include <mtfs_dentry.h>
#include <mtfs_inode.h>
#include <mtfs_file.h>
#include <mtfs_junction.h>
#include "ext_support.h"
#include <mtfs_flag.h>

struct lowerfs_operations lowerfs_ext2_ops = {
	lowerfs_owner:           THIS_MODULE,
	lowerfs_type:            "ext2",
	lowerfs_magic:           EXT2_SUPER_MAGIC,
	lowerfs_flag:            0,
	lowerfs_inode_set_flag:  lowerfs_inode_set_flag_default,
	lowerfs_inode_get_flag:  lowerfs_inode_get_flag_default,
	lowerfs_idata_init:      NULL,
	lowerfs_idata_finit:     NULL,
};

struct lowerfs_operations lowerfs_ext3_ops = {
	lowerfs_owner:           THIS_MODULE,
	lowerfs_type:            "ext3",
	lowerfs_magic:           EXT3_SUPER_MAGIC,
	lowerfs_flag:            0,
	lowerfs_inode_set_flag:  lowerfs_inode_set_flag_default,
	lowerfs_inode_get_flag:  lowerfs_inode_get_flag_default,
	lowerfs_idata_init:      NULL,
	lowerfs_idata_finit:     NULL,
};

struct lowerfs_operations lowerfs_ext4_ops = {
	lowerfs_owner:           THIS_MODULE,
	lowerfs_type:            "ext4",
	lowerfs_magic:           EXT4_SUPER_MAGIC,
	lowerfs_flag:            0,
	lowerfs_inode_set_flag:  lowerfs_inode_set_flag_default,
	lowerfs_inode_get_flag:  lowerfs_inode_get_flag_default,
	lowerfs_idata_init:      NULL,
	lowerfs_idata_finit:     NULL,
};

static int ext_support_init(void)
{
	int ret = 0;

	MDEBUG("registering mtfs_ext lowerfs support\n");

	ret = lowerfs_register_ops(&lowerfs_ext2_ops);
	if (ret) {
		MERROR("failed to register lowerfs operation for ext2: error %d\n", ret);
		goto out;
	}

	ret = lowerfs_register_ops(&lowerfs_ext3_ops);
	if (ret) {
		MERROR("failed to register lowerfs operation for ext3: error %d\n", ret);
		goto out_unregister_ext2;
	}

	ret = lowerfs_register_ops(&lowerfs_ext4_ops);
	if (ret) {
		MERROR("failed to register lowerfs operation for ext4: error %d\n", ret);
		goto out_unregister_ext3;
	}
	goto out;
out_unregister_ext3:
	lowerfs_unregister_ops(&lowerfs_ext3_ops);
out_unregister_ext2:
	lowerfs_unregister_ops(&lowerfs_ext2_ops);
out:
	return ret;
}

static void ext_support_exit(void)
{
	MDEBUG("unregistering mtfs_ext2 lowerfs support\n");
	lowerfs_unregister_ops(&lowerfs_ext2_ops);
	lowerfs_unregister_ops(&lowerfs_ext3_ops);
	lowerfs_unregister_ops(&lowerfs_ext4_ops);
}

MODULE_AUTHOR("MulTi File System Workgroup");
MODULE_DESCRIPTION("mtfs's support for ext2");
MODULE_LICENSE("GPL");

module_init(ext_support_init);
module_exit(ext_support_exit);
