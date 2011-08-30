/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
 */

#ifndef __HRFS_BITMAP_H__
#define __HRFS_BITMAP_H__
#include <libcfs/user-bitops.h>
#include <libcfs/bitmap.h>

static inline void cfs_bitmap_dump(bitmap_t *bitmap)
{
	int i = 0;
	cfs_foreach_bit(bitmap, i) {
		HPRINT("%d ", i);
	}
	HPRINT("\n");
}

static inline bitmap_t *cfs_bitmap_allocate(int size)
{
	bitmap_t *ptr = NULL;

	HRFS_ALLOC(ptr, CFS_BITMAP_SIZE(size));
	if (ptr == NULL) {
		return (ptr);
	}

	/* Clear all bits */
	memset(ptr, 0, CFS_BITMAP_SIZE(size));
	ptr->size = size;

	return (ptr);
}

#define cfs_bitmap_freee(ptr) HRFS_FREE(ptr, CFS_BITMAP_SIZE(ptr->size));
#endif /* __HRFS_BITMAP_H__ */

