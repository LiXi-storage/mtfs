/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <linux/module.h>
#include <linux/quota.h>
#include <linux/quotaops.h>
#include <debug.h>
#include <mtfs_lowerfs.h>

struct mtfs_lowerfs lowerfs_ext2 = {
	ml_owner:           THIS_MODULE,
	ml_type:            "ext2",
	ml_magic:           EXT2_SUPER_MAGIC,
	ml_bucket_type:     &mlowerfs_bucket_xattr,
	ml_flag:            0,
	ml_setflag:         mlowerfs_setflag_default,
	ml_getflag:         mlowerfs_getflag_default,
};
