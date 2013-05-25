/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <mtfs_lowerfs.h>
#include <mtfs_super.h>
#include <mtfs_dentry.h>
#include <mtfs_inode.h>
#include <mtfs_mmap.h>
#include <mtfs_stack.h>
#include <mtfs_junction.h>
#include <linux/module.h>

#define TMPFS_MAGIC 0x01021994

static struct mtfs_lowerfs lowerfs_tmpfs = {
	ml_owner:           THIS_MODULE,
	ml_type:            "tmpfs",
	ml_magic:           TMPFS_MAGIC,
	ml_bucket_type:    &mlowerfs_bucket_nop,
	ml_features:        0,
	ml_setflag:         mlowerfs_setflag_nop,
	ml_getflag:         mlowerfs_getflag_nop,
};

static int lowerfs_tmpfs_init(void)
{
	int ret = 0;

	MDEBUG("mtfs lowerfs support for tmpfs\n");

	ret = mlowerfs_register(&lowerfs_tmpfs);
	if (ret) {
		MERROR("failed to register lowerfs operation: error %d\n", ret);
	}	

	return ret;
}

static void lowerfs_tmpfs_exit(void)
{
	MDEBUG("unregistering mtfs lowerfs support for tmpfs\n");
	mlowerfs_unregister(&lowerfs_tmpfs);
}

MODULE_AUTHOR("MulTi File System Workgroup");
MODULE_DESCRIPTION("mtfs's support for tmpfs");
MODULE_LICENSE("GPL");

module_init(lowerfs_tmpfs_init);
module_exit(lowerfs_tmpfs_exit);
