/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
 */

#ifndef __HRFS_FILE_INTERNAL_H__
#define __HRFS_FILE_INTERNAL_H__
#include <hrfs_file.h>

static inline int hrfs_f_alloc(struct file *file, int bnum)
{
	struct hrfs_file_info *f_info = NULL;
	int ret = 0;
	
	HASSERT(file);
	HASSERT(bnum > 0 && bnum <= HRFS_BRANCH_MAX);

	HRFS_SLAB_ALLOC_PTR(f_info, hrfs_file_info_cache);
	if (unlikely(f_info == NULL)) {
		ret = -ENOMEM;
		goto out;
	}
	f_info->bnum = bnum;
	
	_hrfs_f2info(file) = f_info;
out:
	return ret;
}

static inline int hrfs_f_free(struct file *file)
{
	struct hrfs_file_info *f_info = NULL;
	int ret = 0;
	
	HASSERT(file);
	f_info = hrfs_f2info(file);
	HASSERT(f_info);

	HRFS_SLAB_FREE_PTR(f_info, hrfs_file_info_cache);
	_hrfs_f2info(file) = NULL;
	HASSERT(_hrfs_f2info(file) == NULL);
	return ret;
}
#endif /* __HRFS_FILE_INTERNAL_H__ */
