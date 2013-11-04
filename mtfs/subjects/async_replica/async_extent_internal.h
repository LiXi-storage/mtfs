/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_ASYNC_EXTENT_INTERNAL_H__
#define __MTFS_ASYNC_EXTENT_INTERNAL_H__

#include <mtfs_async.h>

struct masync_extent *masync_extent_init(struct masync_bucket *bucket);
void masync_extent_get(struct masync_extent *async_extent);
void masync_extent_put(struct masync_extent *async_extent);

static inline void masync_extent_add_to_lru(struct masync_extent *extent)
{
	struct msubject_async_info *info = NULL;
	MENTRY();

	info = extent->mae_bucket->mab_info;

	mtfs_spin_lock(&info->msai_lru_lock);
	MASSERT(mtfs_list_empty(&extent->mae_lru_linkage));
	mtfs_list_add_tail(&extent->mae_lru_linkage, &info->msai_lru_extents);
	atomic_inc(&info->msai_lru_number);
	mtfs_spin_unlock(&info->msai_lru_lock);

	masync_extent_get(extent);

	_MRETURN();
}

#if 0
static inline void masync_extent_touch_in_lru(struct masync_extent *extent)
{
	struct msubject_async_info *info = NULL;
	MENTRY();

	info = extent->mae_bucket->mab_info;

	mtfs_spin_lock(&info->msai_lru_lock);
	MASSERT(!mtfs_list_empty(&extent->mae_lru_linkage));
	mtfs_list_move_tail(&extent->mae_lru_linkage, &info->msai_lru_extents);
	mtfs_spin_unlock(&info->msai_lru_lock);

	_MRETURN();
}
#endif

/*
 * Reutrn 1 if succeeded, 0 if not in the LRU list.
 */
static inline int masync_extent_remove_from_lru_nonlock(struct masync_extent *extent)
{
	struct msubject_async_info *info = NULL;
	int ret = 0;
	MENTRY();

	info = extent->mae_info;

	if (!mtfs_list_empty(&extent->mae_lru_linkage)) {
		mtfs_list_del_init(&extent->mae_lru_linkage);
		atomic_dec(&info->msai_lru_number);
		ret = 1;
	}

	MRETURN(ret);
}

/*
 * Before calling this function, masync_extent_get() first.
 */
static inline void masync_extent_remove_from_lru(struct masync_extent *extent)
{
	struct msubject_async_info *info = NULL;
	int need_put = 0;
	MENTRY();

	info = extent->mae_info;

	mtfs_spin_lock(&info->msai_lru_lock);
	need_put = masync_extent_remove_from_lru_nonlock(extent);
	mtfs_spin_unlock(&info->msai_lru_lock);

	if (need_put) {
		masync_extent_put(extent);
	}

	_MRETURN();
}

int masync_extent_flush(struct masync_extent *async_extent,
			char *buf,
                        int buf_size);
#endif /* __MTFS_ASYNC_EXTENT_INTERNAL_H__ */
