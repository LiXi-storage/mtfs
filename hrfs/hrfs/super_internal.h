/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
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

static inline hrfs_s_info_t *hrfs_si_alloc(void)
{
	hrfs_s_info_t *s_info = NULL;

	HRFS_SLAB_ALLOC_PTR(s_info, hrfs_sb_info_cache);
	return s_info;
}

static inline void hrfs_si_free(hrfs_s_info_t *s_info)
{
	HRFS_SLAB_FREE_PTR(s_info, hrfs_sb_info_cache);
	HASSERT(s_info == NULL);
}

static inline int hrfs_si_branch_alloc(hrfs_s_info_t *s_info, hrfs_bindex_t bnum)
{
	int ret = -ENOMEM;

	HASSERT(s_info);

	HRFS_ALLOC(s_info->si_branch, sizeof(*s_info->si_branch) * bnum);
	if (unlikely(s_info->si_branch == NULL)) {
		goto out;
	}

	HRFS_ALLOC(s_info->si_mnt_branch, sizeof(*s_info->si_mnt_branch) * bnum);	
	if (unlikely(s_info->si_mnt_branch == NULL)) {
		goto out_free_branch;
	}

	HRFS_ALLOC(s_info->si_d_recover, sizeof(*s_info->si_d_recover) * bnum);	
	if (unlikely(s_info->si_d_recover == NULL)) {
		goto out_free_mnt;
	}

	s_info->si_bnum = bnum;
	ret = 0;
	goto out;
out_free_mnt:
	HRFS_FREE(s_info->si_mnt_branch, sizeof(*s_info->si_mnt_branch) * bnum);
out_free_branch:
	HRFS_FREE(s_info->si_branch, sizeof(*s_info->si_branch) * bnum);
out:
	return ret;
}

static inline int hrfs_si_branch_free(hrfs_s_info_t *s_info)
{
	int ret = 0;

	HASSERT(s_info);
	HASSERT(s_info->si_branch);

	HRFS_FREE(s_info->si_branch, sizeof(*s_info->si_branch) * s_info->si_bnum);
	HRFS_FREE(s_info->si_mnt_branch, sizeof(*s_info->si_mnt_branch) * s_info->si_bnum);
	HRFS_FREE(s_info->si_d_recover, sizeof(*s_info->si_d_recover) * s_info->si_bnum);
	//s_info->si_branch = NULL; /* HRFS_FREE will do this */
	//s_info->si_mnt_branch = NULL; /* HRFS_FREE will do this */
	HASSERT(s_info->si_branch == NULL);
	HASSERT(s_info->si_mnt_branch == NULL);
	HASSERT(s_info->si_d_recover == NULL);
	return ret;
}

static inline int hrfs_s_alloc(struct super_block *sb, hrfs_bindex_t bnum)
{
	hrfs_s_info_t *s_info = NULL;
	int ret = 0;

	HASSERT(sb);

	s_info = hrfs_si_alloc();
	if (unlikely(s_info == NULL)) {
		ret = -ENOMEM;
		goto out;
	}

	ret = hrfs_si_branch_alloc(s_info, bnum);
	if (unlikely(ret)) {
		goto out_sinfo;
	}

	_hrfs_s2info(sb) = s_info;
	goto out;
out_sinfo:
	hrfs_si_free(s_info);
out:
	return ret;
}

static inline int hrfs_s_free(struct super_block *sb)
{
	hrfs_s_info_t *s_info = NULL;
	int ret = 0;

	HASSERT(sb);
	s_info = hrfs_s2info(sb);
	HASSERT(s_info);

	hrfs_si_branch_free(s_info);

	hrfs_si_free(s_info);
	_hrfs_s2info(sb) = NULL;
	HASSERT(_hrfs_s2info(sb) == NULL);
	return ret;
}

#endif /* __HRFS_SUPER_INTERNAL_H__ */
