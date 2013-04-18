/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <linux/module.h>
#include <mtfs_heal.h>
#include <mtfs_lowerfs.h>
#include <mtfs_super.h>
#include <mtfs_dentry.h>
#include <mtfs_inode.h>
#include <mtfs_file.h>
#include <mtfs_junction.h>
#include <mtfs_flag.h>
#include "lowerfs_ext.h"
#include "lowerfs_ext3.h"
#include "lowerfs_ext4.h"

struct mtfs_lowerfs lowerfs_ext2 = {
	ml_owner:           THIS_MODULE,
	ml_type:            "ext2",
	ml_magic:           EXT2_SUPER_MAGIC,
	ml_bucket_type:     &mlowerfs_bucket_xattr,
	ml_flag:            0,
	ml_setflag:         mlowerfs_setflag_default,
	ml_getflag:         mlowerfs_getflag_default,
};

static int ext_support_init(void)
{
	int ret = 0;

	MDEBUG("registering mtfs_ext lowerfs support\n");

	ret = mlowerfs_register(&lowerfs_ext2);
	if (ret) {
		MERROR("failed to register lowerfs for ext2: error %d\n", ret);
		goto out;
	}

	ret = mlowerfs_register(&lowerfs_ext3);
	if (ret) {
		MERROR("failed to register lowerfs for ext3: error %d\n", ret);
		goto out_unregister_ext2;
	}

	ret = mlowerfs_register(&lowerfs_ext4);
	if (ret) {
		MERROR("failed to register lowerfs for ext4: error %d\n", ret);
		goto out_unregister_ext3;
	}
	goto out;
out_unregister_ext3:
	mlowerfs_unregister(&lowerfs_ext3);
out_unregister_ext2:
	mlowerfs_unregister(&lowerfs_ext2);
out:
	return ret;
}

static void ext_support_exit(void)
{
	MDEBUG("unregistering mtfs_ext2 lowerfs support\n");
	mlowerfs_unregister(&lowerfs_ext2);
	mlowerfs_unregister(&lowerfs_ext3);
	mlowerfs_unregister(&lowerfs_ext4);
}

MODULE_AUTHOR("MulTi File System Workgroup");
MODULE_DESCRIPTION("mtfs's support for ext2");
MODULE_LICENSE("GPL");

module_init(ext_support_init);
module_exit(ext_support_exit);
