/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __HRFS_SUPER_INTERNAL_H__
#define __HRFS_SUPER_INTERNAL_H__
#include <memory.h>
#include <debug.h>
#include <hrfs_super.h>
//#include "hrfs_internal.h"
#include "main_internal.h"
extern struct super_operations hrfs_sops;

#define s_type(sb) ((sb)->s_type->name)

/* fist file systems superblock magic */
#define HRFS_SUPER_MAGIC 0xc0ffee

/* hrfs root inode number */
#define HRFS_ROOT_INO	 1

static inline int hrfs_s_alloc(struct super_block *sb, hrfs_bindex_t bnum)
{
	struct hrfs_sb_info *s_info = NULL;
	int ret = 0;

	HASSERT(sb);
	HASSERT(bnum > 0 && bnum <= HRFS_BRANCH_MAX);

	HRFS_SLAB_ALLOC_PTR(s_info, hrfs_sb_info_cache);
	if (unlikely(s_info == NULL)) {
		ret = -ENOMEM;
		goto out;
	}
	s_info->bnum = bnum;

	_hrfs_s2info(sb) = s_info;
out:
	return ret;
}

static inline int hrfs_s_free(struct super_block *sb)
{
	struct hrfs_sb_info *s_info = NULL;
	int ret = 0;

	HASSERT(sb);
	s_info = hrfs_s2info(sb);
	HASSERT(s_info);

	HRFS_SLAB_FREE_PTR(s_info, hrfs_sb_info_cache);
	_hrfs_s2info(sb) = NULL;
	HASSERT(_hrfs_s2info(sb) == NULL);
	return ret;
}

#endif /* __HRFS_SUPER_INTERNAL_H__ */
