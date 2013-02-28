/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_LOWERFS_INTERNAL_H__
#define __MTFS_LOWERFS_INTERNAL_H__
#include <raid.h>
#include <mtfs_common.h>
#include <mtfs_lowerfs.h>
#include <mtfs_flag.h>

struct mtfs_lowerfs *mlowerfs_get(const char *type);
void mlowerfs_put(struct mtfs_lowerfs *fs_ops);

static inline int mlowerfs_get_raid_type(struct mtfs_lowerfs *fs_ops, struct inode *inode, raid_type_t *raid_type)
{
	int ret = 0;
	__u32 mtfs_flag = 0;

	if (fs_ops->ml_getflag) {
		ret = fs_ops->ml_getflag(inode, &mtfs_flag);
		if (ret) {
			goto out;
		}
	}

	if (mtfs_flag & MTFS_FLAG_SETED) {
		*raid_type = mtfs_flag & MTFS_FLAG_RAID_MASK;
	} else {
		*raid_type = -1;
	}
out:
	return ret;
}

int mlowerfs_setflag(struct mtfs_lowerfs *lowerfs,
                     struct inode *inode,
                     __u32 mtfs_flag);
int mlowerfs_getflag(struct mtfs_lowerfs *lowerfs,
                     struct inode *inode,
                     __u32 *mtfs_flag);
int mlowerfs_invalidate(struct mtfs_lowerfs *lowerfs,
                        struct inode *inode,
                        __u32 valid_flags);
#endif /* __MTFS_LOWERFS_INTERNAL_H__ */
