/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <linux/module.h>
#include <mtfs_lowerfs.h>
#include <mtfs_flag.h>
#include "ntfs3g_support.h"

#define MLOWERFS_XATTR_NAME_FLAG_NTFS3G "user."MLOWERFS_XATTR_NAME_FLAG

int mlowerfs_getflag_ntfs3g(struct inode *inode, __u32 *mtfs_flag)
{
	int ret = 0;
	ret = mlowerfs_getflag_xattr(inode, mtfs_flag, MLOWERFS_XATTR_NAME_FLAG_NTFS3G);
	MRETURN(ret);
}
EXPORT_SYMBOL(mlowerfs_getflag_ntfs3g);

int mlowerfs_setflag_ntfs3g(struct inode *inode, __u32 mtfs_flag)
{
	int ret = 0;
	ret = mlowerfs_setflag_xattr(inode, mtfs_flag, MLOWERFS_XATTR_NAME_FLAG_NTFS3G);
	MRETURN(ret);
}
EXPORT_SYMBOL(mlowerfs_setflag_ntfs3g);

struct mtfs_lowerfs lowerfs_ntfs3g = {
	ml_owner:           THIS_MODULE,
	ml_type:            "fuseblk",
	ml_magic:           NTFS_SB_MAGIC,
	ml_features:        0,
	ml_setflag:         mlowerfs_setflag_ntfs3g,
	ml_getflag:         mlowerfs_getflag_ntfs3g,
};

static int ntfs3g_support_init(void)
{
	int ret = 0;

	MDEBUG("registering lowerfs support for ntfs3g\n");

	ret = mlowerfs_register(&lowerfs_ntfs3g);
	if (ret) {
		MERROR("failed to register lowerfs operation: error %d\n", ret);
	}	

	return ret;
}

static void ntfs3g_support_exit(void)
{
	MDEBUG("unregistering lowerfs support for ntfs3g\n");
	mlowerfs_unregister(&lowerfs_ntfs3g);
}

MODULE_AUTHOR("MulTi File System Workgroup");
MODULE_DESCRIPTION("mtfs's lowerfs support for ntfs3g");
MODULE_LICENSE("GPL");

module_init(ntfs3g_support_init);
module_exit(ntfs3g_support_exit);
