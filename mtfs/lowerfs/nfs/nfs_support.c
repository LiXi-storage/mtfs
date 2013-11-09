/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <mtfs_lowerfs.h>
#include <mtfs_super.h>
#include <mtfs_dentry.h>
#include <mtfs_inode.h>
#include <mtfs_file.h>
#include <mtfs_mmap.h>
#include <mtfs_junction.h>
#include <linux/module.h>
#include "nfs_support.h"

struct mtfs_lowerfs lowerfs_nfs = {
	ml_owner:           THIS_MODULE,
	ml_type:            "nfs",
	ml_magic:           NFS_SUPER_MAGIC,
	ml_flag:            0,
	ml_setflag:         mlowerfs_setflag_nop,
	ml_getflag:         mlowerfs_getflag_nop,
};

static int nfs_support_init(void)
{
	int ret = 0;

	MDEBUG("registering lowerfs support for nfs\n");

	ret = mlowerfs_register(&lowerfs_nfs);
	if (ret) {
		MERROR("failed to register lowerfs operation: error %d\n", ret);
	}	

	return ret;
}

static void nfs_support_exit(void)
{
	MDEBUG("unregistering lowerfs support for nfs\n");
	mlowerfs_unregister(&lowerfs_nfs);
}

MODULE_AUTHOR("MulTi File System Workgroup");
MODULE_DESCRIPTION("mtfs's lowerfs support for nfs");
MODULE_LICENSE("GPL");

module_init(nfs_support_init);
module_exit(nfs_support_exit);
