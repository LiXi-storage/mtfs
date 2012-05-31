/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_FILE_INTERNAL_H__
#define __MTFS_FILE_INTERNAL_H__
#include <mtfs_file.h>

static inline int mtfs_f_alloc(struct file *file, int bnum)
{
	struct mtfs_file_info *f_info = NULL;
	int ret = 0;
	
	HASSERT(file);
	HASSERT(bnum > 0 && bnum <= MTFS_BRANCH_MAX);

	MTFS_SLAB_ALLOC_PTR(f_info, mtfs_file_info_cache);
	if (unlikely(f_info == NULL)) {
		ret = -ENOMEM;
		goto out;
	}
	f_info->bnum = bnum;
	
	_mtfs_f2info(file) = f_info;
out:
	return ret;
}

static inline int mtfs_f_free(struct file *file)
{
	struct mtfs_file_info *f_info = NULL;
	int ret = 0;
	
	HASSERT(file);
	f_info = mtfs_f2info(file);
	HASSERT(f_info);

	MTFS_SLAB_FREE_PTR(f_info, mtfs_file_info_cache);
	_mtfs_f2info(file) = NULL;
	HASSERT(_mtfs_f2info(file) == NULL);
	return ret;
}
#endif /* __MTFS_FILE_INTERNAL_H__ */
