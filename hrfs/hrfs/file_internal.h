/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
 */

#ifndef __HRFS_FILE_INTERNAL_H__
#define __HRFS_FILE_INTERNAL_H__
#include <hrfs_file.h>

static inline hrfs_f_info_t *hrfs_fi_alloc(void)
{
	hrfs_f_info_t *f_info = NULL;
	
	HRFS_SLAB_ALLOC_PTR(f_info, hrfs_file_info_cache);	
	return f_info;
}

static inline void hrfs_fi_free(hrfs_f_info_t *f_info)
{
	HRFS_SLAB_FREE_PTR(f_info, hrfs_file_info_cache);
	HASSERT(f_info == NULL);
}

static inline int hrfs_fi_branch_alloc(hrfs_f_info_t *f_info, int bnum)
{
	int ret = -ENOMEM;
	
	HASSERT(f_info);
	
	HRFS_ALLOC(f_info->barray, sizeof(*f_info->barray) * bnum);
	if (unlikely(f_info->barray == NULL)) {
		goto out;
	}

	f_info->bnum = bnum;
	ret = 0;	
out:
	return ret;
}

static inline int hrfs_fi_branch_free(hrfs_f_info_t *f_info)
{
	int ret = 0;
	
	HASSERT(f_info);
	HASSERT(f_info->barray);
	
	HRFS_FREE(f_info->barray, sizeof(*f_info->barray) * f_info->bnum);
	
	//f_info->fi_branch = NULL; /* HRFS_FREE will do this */
	HASSERT(f_info->barray == NULL);
	return ret;
}

static inline int hrfs_f_alloc(struct file *file, int bnum)
{
	hrfs_f_info_t *f_info = NULL;
	int ret = 0;
	
	HASSERT(file);
	
	f_info = hrfs_fi_alloc();
	if (unlikely(f_info == NULL)) {
		ret = -ENOMEM;
		goto out;
	}
	
	ret = hrfs_fi_branch_alloc(f_info, bnum);
	if (unlikely(ret)) {
		goto out_finfo;
	}
	
	_hrfs_f2info(file) = f_info;
	ret = 0;
	goto out;
out_finfo:
	hrfs_fi_free(f_info);
out:
	return ret;
}

static inline int hrfs_f_free(struct file *file)
{
	hrfs_f_info_t *f_info = NULL;
	int ret = 0;
	
	HASSERT(file);
	f_info = hrfs_f2info(file);
	HASSERT(f_info);
	
	hrfs_fi_branch_free(f_info);
	
	hrfs_fi_free(f_info);
	_hrfs_f2info(file) = NULL;
	HASSERT(_hrfs_f2info(file) == NULL);
	return ret;
}
#endif /* __HRFS_FILE_INTERNAL_H__ */
