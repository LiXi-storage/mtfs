/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <debug.h>
#include <mtfs_lowerfs.h>
#include "lowerfs_lustre.h"

struct mtfs_lowerfs lowerfs_lustre = {
	ml_owner:           THIS_MODULE,
	ml_type:            "lustre",
	ml_magic:           LUSTRE_SUPER_MAGIC,
	ml_bucket_type:    &mlowerfs_bucket_xattr,
	ml_features:        0,
	ml_setflag:         mlowerfs_setflag_default,
	ml_getflag:         mlowerfs_getflag_default,
};

static int lowerfs_lustre_init(void)
{
	int ret = 0;

	MDEBUG("registering mtfs lowerfs for lustre\n");

	ret = mlowerfs_register(&lowerfs_lustre);
	if (ret) {
		MERROR("failed to register lowerfs for lustre, ret = %d\n", ret);
	}

	return ret;
}

static void lowerfs_lustre_exit(void)
{
	MDEBUG("unregistering mtfs lowerfs for lustre\n");
	mlowerfs_unregister(&lowerfs_lustre);
}

MODULE_AUTHOR("MulTi File System Workgroup");
MODULE_DESCRIPTION("mtfs's support for lustre");
MODULE_LICENSE("GPL");

module_init(lowerfs_lustre_init);
module_exit(lowerfs_lustre_exit);
