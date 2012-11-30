/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_ASYNC_BUCKET_INTERNAL_H__
#define __MTFS_ASYNC_BUCKET_INTERNAL_H__
#include "async_internal.h"

static inline void masync_bucket_lock_init(struct masync_bucket *bucket)
{
	init_MUTEX(&bucket->mab_lock);
}

/* Returns 0 if the mutex has
 * been acquired successfully
 * or 1 if it it cannot be acquired.
 */
static inline int masync_bucket_trylock(struct masync_bucket *bucket)
{
	return down_trylock(&bucket->mab_lock);
}

static inline void masync_bucket_lock(struct masync_bucket *bucket)
{
	down(&bucket->mab_lock);
}

static inline void masync_bucket_unlock(struct masync_bucket *bucket)
{
	up(&bucket->mab_lock);
}

static inline void masync_bucket_remove_from_lru_nonlock(struct masync_bucket *bucket)
{
	struct msubject_async_info *info = NULL;
	MENTRY();

	info = bucket->mab_info;
	MASSERT(info);
	MASSERT(atomic_read(&bucket->mab_nr) == 0);
	MASSERT(!mtfs_list_empty(&bucket->mab_linkage));
	MASSERT(bucket->mab_root == NULL);
	mtfs_list_del_init(&bucket->mab_linkage);

	_MRETURN();
}

static inline void masync_bucket_remove_from_lru(struct masync_bucket *bucket)
{
	struct msubject_async_info *info = NULL;
	MENTRY();

	info = bucket->mab_info;
	MASSERT(info);
	MASSERT(atomic_read(&bucket->mab_nr) == 0);
	MASSERT(!mtfs_list_empty(&bucket->mab_linkage));
	MASSERT(bucket->mab_root == NULL);
	masync_info_write_lock(bucket->mab_info);
	masync_bucket_remove_from_lru_nonlock(bucket);
	masync_info_write_unlock(bucket->mab_info);

	_MRETURN();
}

static inline void masync_bucket_add_to_lru_nonlock(struct masync_bucket *bucket)
{
	struct msubject_async_info *info = NULL;
	MENTRY();

	info = bucket->mab_info;
	MASSERT(info);
	MASSERT(mtfs_list_empty(&bucket->mab_linkage));
	MASSERT(bucket->mab_root != NULL);
	MASSERT(atomic_read(&bucket->mab_nr) != 0);
	mtfs_list_add_tail(&bucket->mab_linkage, &info->msai_dirty_buckets);

	_MRETURN();
}

static inline void masync_bucket_add_to_lru(struct masync_bucket *bucket)
{
	struct msubject_async_info *info = NULL;
	MENTRY();

	info = bucket->mab_info;
	MASSERT(info);
	MASSERT(mtfs_list_empty(&bucket->mab_linkage));
	MASSERT(bucket->mab_root != NULL);
	MASSERT(atomic_read(&bucket->mab_nr) != 0);
	masync_info_write_lock(info);
	masync_bucket_add_to_lru_nonlock(bucket);
	masync_info_write_unlock(info);

	_MRETURN();
}

static inline void masync_bucket_degrade_in_lru_nonlock(struct masync_bucket *bucket)
{
	struct msubject_async_info *info = NULL;
	MENTRY();

	info = bucket->mab_info;
	MASSERT(info);
	MASSERT(!mtfs_list_empty(&bucket->mab_linkage));
	mtfs_list_move(&bucket->mab_linkage, &info->msai_dirty_buckets);

	_MRETURN();
}

static inline void masync_bucket_touch_in_lru_nonlock(struct masync_bucket *bucket)
{
	struct msubject_async_info *info = NULL;
	MENTRY();

	info = bucket->mab_info;
	MASSERT(info);
	MASSERT(!mtfs_list_empty(&bucket->mab_linkage));
	mtfs_list_move_tail(&bucket->mab_linkage, &info->msai_dirty_buckets);

	_MRETURN();
}

static inline void masync_bucket_touch_in_lru(struct masync_bucket *bucket)
{
	struct msubject_async_info *info = NULL;
	MENTRY();

	info = bucket->mab_info;
	MASSERT(info);
	MASSERT(!mtfs_list_empty(&bucket->mab_linkage));
	masync_info_write_lock(info);
	masync_bucket_touch_in_lru_nonlock(bucket);
	masync_info_write_unlock(info);

	_MRETURN();
}

int masync_bucket_add(struct file *file,
                      struct mtfs_interval_node_extent *extent);
int masync_bucket_cancel(struct masync_bucket *bucket,
                         char *buf, int buf_len,
                         int nr_to_cacel);
void masync_bucket_init(struct msubject_async_info *info,
                        struct masync_bucket *bucket);
int masync_bucket_cleanup(struct masync_bucket *bucket);
#endif /* __MTFS_ASYNC_BUCKET_INTERNAL_H__ */
